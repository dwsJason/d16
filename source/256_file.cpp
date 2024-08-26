//
// C++ Encoder/Decoder
// For C256 Foenix Picture File Format
// 
//  $$TODO, bring down a text file copy of the spec into the project
// https://docs.google.com/document/d/10ovgMClDAJVgbW0sOhUsBkVABKWhOPM5Au7vbHJymoA/edit?usp=sharing
//
#include "256_file.h"

#include <stdio.h>

// I have to say, it's a good thing that lzsa2 has good compression ratios
// because these include names are fucking terrible

//lzsa numeric includes
#include "lib.h"
//lzsa memory compressor
#include "shrink_inmem.h"
//lzsa memory decompressor
#include "expand_inmem.h"

// If these structs are the wrong size, there's an issue with type sizes, and
// your compiler
static_assert(sizeof(C256File_Header)==16, "C256File_Header is supposed to be 16 bytes");
static_assert(sizeof(C256File_CLUT)==10,  "C256File_CLUT is supposed to be 10 bytes");
static_assert(sizeof(C256File_PIXL)==10, "C256File_PIXL is supposed to be 10 bytes");
static_assert(sizeof(C256File_CHUNK)==8, "C256File_CHUNK is supposed to be 8 bytes");

//------------------------------------------------------------------------------
// Load in a C256File constructor
//
C256File::C256File(const char *pFilePath)
	: m_widthPixels(0)
	, m_heightPixels(0)
	, m_numColors( 0 )
{

	m_pal.iNumColors = 0;
	m_pal.pColors = nullptr;

	LoadFromFile(pFilePath);
}
//------------------------------------------------------------------------------
// Create a blank C256File constructor
//
C256File::C256File(int iWidthPixels, int iHeightPixels, int iNumColors)
	: m_widthPixels( iWidthPixels )
	, m_heightPixels( iHeightPixels )
	, m_numColors( iNumColors )
{

	m_pal.iNumColors = iNumColors;
	m_pal.pColors = new C256_Color[ iNumColors ];
}

C256File::~C256File()
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

void C256File::SetPalette( const C256_Palette& palette )
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
void C256File::AddImages( const std::vector<unsigned char*>& pPixelMaps )
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
// If we're trying to save out a series of images, instead make a giant
// vertical film-strip
//
void C256File::CombinePixelMaps()
{
	int numFrames = (int)m_pPixelMaps.size();

	if (numFrames > 1)
	{
		int numPixelsPerFrame = m_widthPixels * m_heightPixels;
		int numPixelsCombined = numPixelsPerFrame * numFrames;

		unsigned char* pPixels = new unsigned char[ numPixelsCombined ];

		// Concatenate the frames

		for (int frameIndex = 0; frameIndex < numFrames; ++frameIndex)
		{
			unsigned char *pDest = pPixels + (numPixelsPerFrame * frameIndex);
			memcpy(pDest, m_pPixelMaps[ frameIndex ], numPixelsPerFrame);
		}

		// Free Up the original memory

		for (int idx = 0; idx < m_pPixelMaps.size(); ++idx)
		{
			delete[] m_pPixelMaps[idx];
			m_pPixelMaps[ idx ] = nullptr;
		}

		// empty the vector
		m_pPixelMaps.clear();

		// insert the new frame
		m_pPixelMaps.push_back( pPixels );

		// adjust the size of the image
		m_heightPixels *= numFrames;

	}
}

//------------------------------------------------------------------------------
//
// Save to File
//
void C256File::SaveToFile(const char* pFilenamePath)
{
	// Handle Animation, by saving out a vertical film-strip
	CombinePixelMaps();

	// Actually, going to serialize to memory, then will save that to file
	std::vector<unsigned char> bytes;

	//--------------------------------------------------------------------------
	// Add the header
	bytes.resize( bytes.size() + sizeof(C256File_Header) );

	//$$JGA Rememeber, you have to set the pointer, before every access
	//$$JGA to the header data, because vector is going to change out
	//$$JGA memory addresses from underneath you
	C256File_Header* pHeader = (C256File_Header*)&bytes[0];

	pHeader->hi = 'I'; pHeader->h2 = '2'; pHeader->h5 = '5'; pHeader->h6 = '6';

	pHeader->file_length = (unsigned int)bytes.size(); // get some valid data in there

	pHeader->version = 0x0000;
	pHeader->width  = m_widthPixels  & 0xFFFF;
	pHeader->height = m_heightPixels & 0xFFFF;
	pHeader->reserved = 0x0000;

	//--------------------------------------------------------------------------
	// Add a CLUT Chunk
	unsigned int clut_size = (m_pal.iNumColors * 4) + sizeof(C256File_CLUT);

	size_t clut_offset = bytes.size();

	size_t decompressed_clut_size = m_pal.iNumColors * 4;

	unsigned char* pCompressedBuffer = new unsigned char[ lzsa_get_max_compressed_size_inmem( decompressed_clut_size ) ];
	unsigned char *pSourceColors = (unsigned char *)m_pal.pColors;

	size_t compSize = lzsa_compress_inmem(pSourceColors,		// input
									   pCompressedBuffer,		// output
									   decompressed_clut_size,  // input size
									   lzsa_get_max_compressed_size_inmem( decompressed_clut_size ),  // max output buffer size
									   LZSA_FLAG_FAVOR_RATIO | LZSA_FLAG_RAW_BLOCK,
									   0,						// minmatchsize (0 better for ratio)
									   2 // Format Version
									   );

	if ((compSize > 0) && (compSize < decompressed_clut_size))
	{
		// Save compressed
		clut_size = (unsigned int) (compSize + sizeof(C256File_CLUT));
		// Add space for the CLUT
		bytes.resize( bytes.size() + clut_size );
		C256File_CLUT* pCLUT = (C256File_CLUT*) &bytes[ clut_offset ];
		pCLUT->c = 'C'; pCLUT->l = 'L'; pCLUT->u = 'U'; pCLUT->t = 'T';
		pCLUT->chunk_length = clut_size;
		pCLUT->num_colors = (unsigned short)(m_pal.iNumColors) | (unsigned short)0x8000; // signal compressed

		memcpy(&bytes[ clut_offset + sizeof(C256File_CLUT) ], pCompressedBuffer,
			   compSize);

	}
	else
	{
		// Save Decompressed
		// Add space for the CLUT
		bytes.resize( bytes.size() + clut_size );
		C256File_CLUT* pCLUT = (C256File_CLUT*) &bytes[ clut_offset ];
		
		pCLUT->c = 'C'; pCLUT->l = 'L'; pCLUT->u = 'U'; pCLUT->t = 'T';
		pCLUT->chunk_length = clut_size;
		pCLUT->num_colors = (unsigned short)(m_pal.iNumColors);

		unsigned char *pBgr = &bytes[ sizeof(C256File_CLUT) + clut_offset ];

		for (int idx = 0; idx < m_pal.iNumColors; ++idx)
		{
			*pBgr++ = m_pal.pColors[ idx ].b;
			*pBgr++ = m_pal.pColors[ idx ].g;
			*pBgr++ = m_pal.pColors[ idx ].r;
			*pBgr++ = m_pal.pColors[ idx ].a;
		}
	}

	delete[] pCompressedBuffer;
	//--------------------------------------------------------------------------
	// Add a PIXL Chunk, which has the pixel data

	size_t pixl_offset = bytes.size();

	// Add space for the PIXL header;
	bytes.resize( bytes.size() + sizeof(C256File_PIXL) );
	C256File_PIXL* pPIXL = (C256File_PIXL*)&bytes[ pixl_offset ];

	pPIXL->p = 'P'; pPIXL->i = 'I'; pPIXL->x = 'X'; pPIXL->l = 'L';
	pPIXL->chunk_length = 0; // Temporary Chunk Size

	size_t decompressed_size = (m_widthPixels * m_heightPixels);

	pPIXL->num_blobs = (short) (decompressed_size / 0x10000);

	// Need to add an extra blob, if we're not a multiple of 65536
	if (decompressed_size & 0xFFFF)
	{
		pPIXL->num_blobs+=1;
	}

	int num_blobs = pPIXL->num_blobs;

	// Work Buffer Guaranteed to be large enough
	unsigned char* pWorkBuffer = new unsigned char[ lzsa_get_max_compressed_size_inmem( 65536 ) ];
	// Grabbing just the first frame
	unsigned char *pSourceData = (unsigned char*)m_pPixelMaps[ 0 ];

	// Compressed Blobs to Follow
	for (int idx = 0; idx < num_blobs; ++idx)
	{
		size_t sourceOffset = 0x10000 * idx;
		int decompressedChunkSize = (int)(decompressed_size - sourceOffset);

		if (decompressedChunkSize > 0x10000)
		{
			decompressedChunkSize = 0x10000;
		}

		compSize = lzsa_compress_inmem(&pSourceData[ sourceOffset ],  // input
								 pWorkBuffer,  	 					  // output
								 decompressedChunkSize,  			  // input size
								 lzsa_get_max_compressed_size_inmem( 65536 ),  // max output buffer size
								 LZSA_FLAG_FAVOR_RATIO | LZSA_FLAG_RAW_BLOCK,
								 0,						// minmatchsize (0 better for ratio)
								 2 // Format Version
								 );


		if (compSize > 0)
		{
			if (compSize >= 0x10000)
			{
				// Signal 64K uncompressed
				bytes.push_back( 0 );
				bytes.push_back( 0 );

				for (int uncompressedIdx = 0; uncompressedIdx < 0x10000; ++uncompressedIdx)
				{
					bytes.push_back((unsigned char) pSourceData[ sourceOffset + uncompressedIdx ]);
				}

			}
			else
			{
				// Add the blob
				bytes.push_back( (compSize>>0) & 0xFF );
				bytes.push_back( (compSize>>8) & 0xFF );

				// is this fast?  Probably not
				for (int compressedIndex = 0; compressedIndex < compSize; ++compressedIndex)
				{
					bytes.push_back((unsigned char)pWorkBuffer[ compressedIndex ]);
				}
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

	// Update the chunk length
	pPIXL = (C256File_PIXL*)&bytes[ pixl_offset ];
	pPIXL->chunk_length = (unsigned int) (bytes.size() - pixl_offset);

	//--------------------------------------------------------------------------
	// Update the header
	pHeader = (C256File_Header*)&bytes[0]; // Required
	pHeader->file_length = (unsigned int)bytes.size(); // get some valid data in there

	// Try not to leak memory, even though we probably do
	//delete[] pWorkBuffer;

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

void C256File::LoadFromFile(const char* pFilePath)
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

		// Bytes are in the buffer, so let's start looking at what we have
		C256File_Header* pHeader = (C256File_Header*) &bytes[0];

		// Early out if things don't look right
		if (!pHeader->IsValid((unsigned int)bytes.size()))
			return;

		m_widthPixels = pHeader->width;
		m_heightPixels = pHeader->height;

		// Go ahead and allocate the bitmap
		size_t frameSize = pHeader->width * pHeader->height;

		// Allocate a Frame
		unsigned char* pFrame = new unsigned char[ frameSize ];
		// Save it in the list
		m_pPixelMaps.push_back(pFrame);

		//----------------------------------------------------------------------
		// Process Chunks as we encounter them
		file_offset += sizeof(C256File_Header);

		// While we're not at the end of the file
		while (file_offset < bytes.size())
		{
			// This is pretty dumb, just get it done
			// These are the types I understand
			// every chunk is supposed to contain a value chunk_length
			// at offset +4, so that we can ignore ones we don't understand
			C256File_CLUT* pCLUT = (C256File_CLUT*)&bytes[ file_offset ];
			C256File_PIXL* pPIXL = (C256File_PIXL*)&bytes[ file_offset ];
			C256File_CHUNK* pCHUNK = (C256File_CHUNK*)&bytes[ file_offset ];

			if (pCLUT->IsValid())
			{
				// We have a CLUT Chunk
				UnpackClut(pCLUT);
			}
			else if (pPIXL->IsValid())
			{
				// We have a PIXeL chunk
				UnpackPixel(pPIXL);
			}

			file_offset += pCHUNK->chunk_length;
		}
	}
}

//------------------------------------------------------------------------------
//
//  Move data out of the CLUT block into the unpacked class structure
//
void C256File::UnpackClut(C256File_CLUT* pCLUT)
{
	int numColors = pCLUT->num_colors & 0x7FFF;

	// Uncompressed
	
	// BGRA Quads
	m_pal.iNumColors = numColors;

	unsigned char* pBGRA = ((unsigned char*) pCLUT) + sizeof(C256File_CLUT);

	m_pal.pColors = new C256_Color[ numColors ];

	if (pCLUT->num_colors & 0x8000)
	{
		// data is compressed
		int version = 2; // format version;
		lzsa_decompress_inmem(pBGRA, 		   // Compressed Data
		  					  (unsigned char *)m_pal.pColors,   // Target uncompressed data
							  pCLUT->chunk_length-sizeof(C256File_CLUT),  // compressed size in bytes
							  numColors * 4,
							  LZSA_FLAG_RAW_BLOCK,
							  &version);


	}
	else
	{
		memcpy(m_pal.pColors, pBGRA, numColors * 4);
	}

	//for (int colorIndex = 0; colorIndex < numColors; ++colorIndex)
	//{
	//	FAN_Color& color = m_pal.pColors[ colorIndex ];
	//
	//	color.b = *pBGR++;
	//	color.g = *pBGR++;
	//	color.r = *pBGR++;
	//	color.a = *pBGR++;
	//}
	
}

//------------------------------------------------------------------------------
//
// Unpack the pixel bitmap, that's been weirdly packed into 64KB chunks
// to make it easier to deal with on 65816
//
void C256File::UnpackPixel(C256File_PIXL* pPIXL)
{
	int num_blobs = pPIXL->num_blobs;

	unsigned char *pData = ((unsigned char*)pPIXL) + sizeof(C256File_PIXL);

	unsigned char *pTargetBuffer = m_pPixelMaps[ 0 ]; // Data needs to be pre allocated

	size_t bufferSize = m_widthPixels * m_heightPixels;

	while (num_blobs-- > 0)
	{
		int compressedSize = *pData++;
		compressedSize |= (*pData++)<<8;

		// Zero Size means 64KB
		if (0 == compressedSize)
		{
			// This means 64KB of uncompressed data
			compressedSize = 0x10000;

			// Copy the non compressed data
			for (int idx = 0; idx < compressedSize; ++idx)
			{
				pTargetBuffer[ idx ] = pData[ idx ];
			}

			bufferSize -= compressedSize;

		}
		else
		{
			int version = 2; // format version;
			size_t decompressedSize = lzsa_decompress_inmem(pData, 		   // Compressed Data
															pTargetBuffer, // Target uncompressed data
															compressedSize, // compressed size in bytes
															bufferSize,
															LZSA_FLAG_RAW_BLOCK,
															&version);

			bufferSize -= decompressedSize;

		}

		pData += compressedSize;
		pTargetBuffer += 0x10000;
	}
}

//------------------------------------------------------------------------------
