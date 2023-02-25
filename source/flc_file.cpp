//
// C++ Decoder
// For FLC/FLI, Autodesk File Format
// 
#include "flc_file.h"
#include <stdio.h>
#include <assert.h>


//------------------------------------------------------------------------------
// Load in a FanFile constructor
//
FlcFile::FlcFile(const char *pFilePath)
	: m_widthPixels(0)
	, m_heightPixels(0)
	, m_numColors( 0 )
{

//	m_pal.iNumColors = 0;
//	m_pal.pColors = nullptr;

	LoadFromFile(pFilePath);
}
//------------------------------------------------------------------------------

FlcFile::~FlcFile()
{
//	if (m_pal.pColors)
//	{
//		delete[] m_pal.pColors;
//		m_pal.pColors = nullptr;
//	}
	// Free Up the memory
	for (int idx = 0; idx < m_pPixelMaps.size(); ++idx)
	{
		delete[] m_pPixelMaps[idx];
		m_pPixelMaps[ idx ] = nullptr;
	}
}

//------------------------------------------------------------------------------

void FlcFile::LoadFromFile(const char* pFilePath)
{
	// Free any existing memory
//	if (m_pal.pColors)
//	{
//		delete[] m_pal.pColors;
//		m_pal.pColors = nullptr;
//	}
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
		//size_t file_offset = 0;	// File Cursor

		m_widthPixels = 320;
		m_heightPixels = 200;
		m_numColors = 256;

		ANM_Header* pHeader = (ANM_Header*)&bytes[0];

		if (pHeader->IsValid())
		{
			//unsigned char* FirstLPage = &bytes[ sizeof(ANM_Header) ];
			// Looks Valid

			// I don't support palette animation, just just grab
			// the palette from the header
			m_pal.iNumColors = 256;
			m_pal.pColors = new ANM_Color[ 256 ];
			for (int idx = 0; idx < 256; ++idx)
			{
				u32 srcColor = pHeader->palette[ idx ];

				ANM_Color color;
				color.r = (u8)(srcColor & 0xFF);
				color.g = (u8)((srcColor >> 8) & 0xFF);
				color.b = (u8)((srcColor >> 16) & 0xFF);
				color.a = 255;

				m_pal.pColors[ idx ] = color;
			}

			// This is another format that applies delta changes
			// to a reference buffer
			unsigned char* pCanvas = new unsigned char[ 320 * 200 ];
			memset(pCanvas, 0, 320*200);

			// Now grab first frame of animation
			unsigned char* pData = FindRecord(pHeader, 0);
			DecodeFrame(pCanvas, pData);

			unsigned char* pFrame = new unsigned char[320 * 200];
			memcpy(pFrame, pCanvas, 320 * 200 );
			m_pPixelMaps.push_back(pFrame);



			// loop through and grab the rest of the frames
			for (int frameNo = 1; frameNo < (int)pHeader->nFrames; ++frameNo)
			{
				pData = FindRecord(pHeader, frameNo);

				// Now we're pointed at compressed data, probably
				DecodeFrame(pCanvas, pData);

				pFrame = new unsigned char[320 * 200];
				memcpy(pFrame, pCanvas, 320 * 200 );
				m_pPixelMaps.push_back(pFrame);
			}
		}
	}
}

//------------------------------------------------------------------------------
