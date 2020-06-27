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
	for (int idx = 0; idx < m_pPixelMaps.size(); ++idx)
	{
		delete[] m_pPixelMaps[idx];
		m_pPixelMaps[ idx ] = nullptr;
	}
}

//------------------------------------------------------------------------------
//
// Make a Copy of the image data
//
//void C2File::AddImages( const std::vector<unsigned char*>& pPixelMaps )
//{
//	int numPixels = m_widthPixels * m_heightPixels;
//
//	for (int idx = 0; idx < pPixelMaps.size(); ++idx)
//	{
//		unsigned char* pPixels = new unsigned char[ numPixels ];
//		memcpy(pPixels, pPixelMaps[ idx ], numPixels);
//		m_pPixelMaps.push_back( pPixels );
//	}
//}

//------------------------------------------------------------------------------

void C2File::LoadFromFile(const char* pFilePath)
{
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
#if 0
		size_t file_offset = 0;	// File Cursor

		// Bytes are in the buffer, so let's start looking at what we have
		FanFile_Header* pHeader = (FanFile_Header*) &bytes[0];

		// Early out if things don't look right
		if (!pHeader->IsValid((unsigned int)bytes.size()))
			return;

		m_widthPixels = pHeader->width;
		m_heightPixels = pHeader->height;

		if (pHeader->version & 0x80)
		{
			// We need to UnSwizzle the Data to "see" it correctly on PC
			// we leave it alone if we're on C256 Device, and use tiles
			printf("$$TODO JGA UnSwizzle Tiles\n");
		}

		// Go ahead and allocate the bitmaps
		unsigned int frameCount = (pHeader->frame_count_high << 16) | pHeader->frame_count;
		unsigned int frameSize = pHeader->width * pHeader->height;

		for (unsigned int idx = 0; idx < frameCount; ++idx)
		{
			unsigned char* pFrame = new unsigned char[ frameSize ];
			m_pPixelMaps.push_back(pFrame);
		}

		//----------------------------------------------------------------------
		// Process Chunks as we encounter them
		file_offset += sizeof(FanFile_Header);

		// While we're not at the end of the file
		while (file_offset < bytes.size())
		{
			// This is pretty dumb, just get it done
			// These are the types I understand
			// every chunk is supposed to contain a value chunk_length
			// at offset +4, so that we can ignore ones we don't understand
			FanFile_CLUT* pCLUT = (FanFile_CLUT*)&bytes[ file_offset ];
			FanFile_INIT* pINIT = (FanFile_INIT*)&bytes[ file_offset ];
			FanFile_FRAM* pFRAM = (FanFile_FRAM*)&bytes[ file_offset ];

			FanFile_CHUNK* pCHUNK = (FanFile_CHUNK*)&bytes[ file_offset ];

			if (pCLUT->IsValid())
			{
				// We have a CLUT Chunk
				UnpackClut(pCLUT);
			}
			else if (pINIT->IsValid())
			{
				// We have an initial frame chunk
				UnpackInitialFrame(pINIT);
			}
			else if (pFRAM->IsValid())
			{
				// We have a packed frames chunk
				UnpackFrames(pFRAM);
			}

			file_offset += pCHUNK->chunk_length;

		}
#endif
	}

}

//------------------------------------------------------------------------------

