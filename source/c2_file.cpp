//
// C++ Encoder/Decoder
// For C2, Paintworks Animation File Format
// 
// File Format summary here from Brutal Deluxe
//
//0000.7fff first pic
//8000.8003 length of frame data - 8008
//8004.8007 timing
//8008.800b length of first frame data
//800c.800d first frame index
//800e.800f first frame data 
//
// Offset 0, Value FFFF indicates the end of a frame
// 	00 00 FF FF 
//
#include "c2_file.h"
#include <stdio.h>

// If these structs are the wrong size, there's an issue with type sizes, and
// your compiler
static_assert(sizeof(C2File_Header)==0x800C, "C2File_Header is supposed to be 0x800C bytes");

//------------------------------------------------------------------------------
// Load in a FanFile constructor
//
C2File::C2File(const char *pFilePath)
	: m_widthPixels(320)
	, m_heightPixels(200)
{
	LoadFromFile(pFilePath);
}
//------------------------------------------------------------------------------

C2File::~C2File()
{
	// Free Up the memory
	for (int idx = 0; idx < m_pC1PixelMaps.size(); ++idx)
	{
		delete[] m_pC1PixelMaps[idx];
		m_pC1PixelMaps[ idx ] = nullptr;
	}
}

//------------------------------------------------------------------------------

void C2File::LoadFromFile(const char* pFilePath)
{
	// Free Up the memory
	for (int idx = 0; idx < m_pC1PixelMaps.size(); ++idx)
	{
		delete[] m_pC1PixelMaps[idx];
		m_pC1PixelMaps[ idx ] = nullptr;
	}

	m_pC1PixelMaps.clear();
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

		// Bytes are in the buffer, so let's start looking at what we have
		C2File_Header* pHeader = (C2File_Header*) &bytes[0];

		// Early out if things don't look right
		if (!pHeader->IsValid((unsigned int)bytes.size()))
			return;

		// Grab Initial Frame, and put it in the list

		unsigned char* pFrame = new unsigned char[ 0x8000 ];
		m_pC1PixelMaps.push_back(pFrame);
		memcpy(pFrame, &bytes[0], 0x8000);

		//----------------------------------------------------------------------
		// Process Frames as we encounter them
		file_offset += sizeof(C2File_Header);

		// While we're not at the end of the file
		while (file_offset < bytes.size())
		{
		}
	}

}

//------------------------------------------------------------------------------

