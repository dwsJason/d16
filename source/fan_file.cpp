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

#include "lz4hc.h" // For the lz4 high-compression implementation

#include <stdio.h>

// If these structs are the wrong size, there's an issue with type sizes, and
// your compiler
static_assert(sizeof(FanFile_Header)==16, "FanFile_Header is supposed to be 16 bytes");
static_assert(sizeof(FanFile_CLUT)==8, "FanFile_CLUT is supposed to be 8 bytes");
static_assert(sizeof(FanFile_FRAM)==8, "FanFile_FRAM is supposed to be 8 bytes");
static_assert(sizeof(FanFile_INIT)==9, "FanFile_INIT is supposed to be 9 bytes");


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
	std::vector<unsigned char> bytes;

	//--------------------------------------------------------------------------
	// Add the header
	bytes.resize( bytes.size() + sizeof(FanFile_Header) );

	//$$JGA Rememeber, you have to set the pointer, before every access
	//$$JGA to the header data, because vector is going to change out
	//$$JGA memory addresses from underneath you
	FanFile_Header* pHeader = (FanFile_Header*)&bytes[0];

	pHeader->f = 'F'; pHeader->a = 'A'; pHeader->n = 'N'; pHeader->m = 'M';
	pHeader->version = 0x00;
	pHeader->width  = m_widthPixels  & 0xFFFF;
	pHeader->height = m_heightPixels & 0xFFFF;
	pHeader->file_length = (unsigned int)bytes.size(); // get some valid data in there

	pHeader->frame_count      = (unsigned short) (m_pPixelMaps.size() & 0xFFFF);
	pHeader->frame_count_high = (unsigned char)  ((m_pPixelMaps.size() >> 16) & 0xFF);

	//--------------------------------------------------------------------------
	// Add a CLUT Chunk

	unsigned int clut_size = (m_pal.iNumColors * 3) + sizeof(FanFile_CLUT);

	size_t clut_offset = bytes.size();

	// Add space for the CLUT
	bytes.resize( bytes.size() + clut_size );
	FanFile_CLUT* pCLUT = (FanFile_CLUT*) &bytes[ clut_offset ];
	
	pCLUT->c = 'C'; pCLUT->l = 'L'; pCLUT->u = 'U'; pCLUT->t = 'T';
	pCLUT->chunk_length = clut_size;

	unsigned char *pBgr = &bytes[ sizeof(FanFile_CLUT) + clut_offset ];

	for (int idx = 0; idx < m_pal.iNumColors; ++idx)
	{
		*pBgr++ = m_pal.pColors[ idx ].b;
		*pBgr++ = m_pal.pColors[ idx ].g;
		*pBgr++ = m_pal.pColors[ idx ].r;
	}

	//--------------------------------------------------------------------------
	// Add an INIT (Initial Frame) Chunk

	size_t init_offset = bytes.size();

	// Add space for the INIT header;
	bytes.resize( bytes.size() + sizeof(FanFile_INIT) );
	FanFile_INIT* pINIT = (FanFile_INIT*)&bytes[ init_offset ];

	pINIT->i = 'I'; pINIT->n = 'N'; pINIT->I = 'I'; pINIT->t = 'T';
	pINIT->chunk_length = 0; // Temporary Chunk Size

	size_t decompressed_size = (m_widthPixels * m_heightPixels);

	pINIT->num_blobs = (unsigned char) (decompressed_size / 0x10000);

	int num_blobs = pINIT->num_blobs;

	// Need to add an extra blob, if we're not a multiple of 65536
	if (decompressed_size & 0xFFFF)
	{
		pINIT->num_blobs+=1;
	}

	// Work Buffer Guaranteed to be large enough
	char* pWorkBuffer = new char[ LZ4_COMPRESSBOUND( 65536 ) ];
	char *pSourceData = (char*)m_pPixelMaps[ 0 ];

	// Compressed Blobs to Follow
	for (int idx = 0; idx < num_blobs; ++idx)
	{
		size_t sourceOffset = 0x10000 * idx;
		int decompressedChunkSize = (int)(decompressed_size - sourceOffset);

		if (decompressedChunkSize > 0x10000)
		{
			decompressedChunkSize = 0x10000;
		}

		int compSize = LZ4_compress_HC(&pSourceData[ sourceOffset ],
								 pWorkBuffer,
								 decompressedChunkSize,
								 LZ4_COMPRESSBOUND( 65536 ),
								 LZ4HC_CLEVEL_MAX );

		if (compSize > 0)
		{
			// Add the blob
			bytes.push_back( (decompressedChunkSize>>0) & 0xFF );
			bytes.push_back( (decompressedChunkSize>>8) & 0xFF );

			// is this fast?  Probably not
			for (int compressedIndex = 0; compressedIndex < compSize; ++compressedIndex)
			{
				bytes.push_back((unsigned char)pWorkBuffer[ compressedIndex ]);
			}

		}
		else
		{
			// FAILED TO COMPRESS
			printf("FAILED TO COMPRESS\n");
			exit(-1);
			return; // just stop
		}
	}

	pINIT = (FanFile_INIT*)&bytes[ init_offset ];

	pINIT->chunk_length = (unsigned int) (bytes.size() - init_offset);


	//--------------------------------------------------------------------------
	// Add a FRAMes Chunk


	//--------------------------------------------------------------------------
	// Update the header
	pHeader = (FanFile_Header*)&bytes[0]; // Required
	pHeader->file_length = (unsigned int)bytes.size(); // get some valid data in there

	//--------------------------------------------------------------------------
	// Create the file and write it
	FILE* pFile = nullptr;
	errno_t err = fopen_s(&pFile, pFilenamePath, "wb");

	if (0==err)
	{
		fwrite(&bytes[0], sizeof(unsigned char), bytes.size(), pFile);
		fclose(pFile);
	}
}

//------------------------------------------------------------------------------

