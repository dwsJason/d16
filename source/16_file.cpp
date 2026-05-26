//
// C++ Encoder/Decoder
// For a GS 16 color index Picture File Format
//
//  $$TODO, bring down a text file copy of the spec into the project
//  $$TODO, spec file for this 16 color file format
// https://docs.google.com/document/d/10ovgMClDAJVgbW0sOhUsBkVABKWhOPM5Au7vbHJymoA/edit?usp=sharing
//
#include "16_file.h"

#include <stdio.h>
#include <string.h>
#include "compat.h"

// I have to say, it's a good thing that lzsa2 has good compression ratios
// because these include names are fucking terrible

//lzsa memory compressor
#include "shrink_inmem.h"
//lzsa memory decompressor
#include "expand_inmem.h"

// LZSA flags (from lib.h, included directly to avoid C++ incompatibility)
#ifndef LZSA_FLAG_FAVOR_RATIO
#define LZSA_FLAG_FAVOR_RATIO    (1<<0)
#define LZSA_FLAG_RAW_BLOCK      (1<<1)
#endif

// If these structs are the wrong size, there's an issue with type sizes, and
// your compiler
static_assert(sizeof(C16_Color)==2,       "C16_Color is supposed to be 2 bytes");
static_assert(sizeof(C16File_Header)==16, "C16File_Header is supposed to be 16 bytes");
static_assert(sizeof(C16File_CLUT)==10,   "C16File_CLUT is supposed to be 10 bytes");
static_assert(sizeof(C16File_PIXL)==10,   "C16File_PIXL is supposed to be 10 bytes");
static_assert(sizeof(C16File_SCBs)==10,   "C16File_SCBs is supposed to be 10 bytes");
static_assert(sizeof(C16File_CHUNK)==8,   "C16File_CHUNK is supposed to be 8 bytes");

//------------------------------------------------------------------------------
// Load in a C16File constructor
//
C16File::C16File(const char *pFilePath)
	: m_widthBytes(0)
	, m_heightPixels(0)
	, m_numColors( 0 )
{

	m_pal.iNumColors = 0;
	m_pal.pColors = nullptr;

	m_scb.iNumScanLines = 0;
	m_scb.pSCB = nullptr;

	LoadFromFile(pFilePath);
}
//------------------------------------------------------------------------------
// Create a blank C16File constructor
//
C16File::C16File(int iWidthBytes, int iHeightPixels, int iNumColors)
	: m_widthBytes( iWidthBytes )
	, m_heightPixels( iHeightPixels )
	, m_numColors( iNumColors )
{

	m_pal.iNumColors = iNumColors;
	m_pal.pColors = new C16_Color[ iNumColors ];

	m_scb.iNumScanLines = 0;
	m_scb.pSCB = nullptr;
}

C16File::~C16File()
{
	if (m_pal.pColors)
	{
		delete[] m_pal.pColors;
		m_pal.pColors = nullptr;
	}
	if (m_scb.pSCB)
	{
		delete[] m_scb.pSCB;
		m_scb.pSCB = nullptr;
	}
	// Free Up the memory
	for (int idx = 0; idx < m_pPixelMaps.size(); ++idx)
	{
		delete[] m_pPixelMaps[idx];
		m_pPixelMaps[ idx ] = nullptr;
	}
}

//------------------------------------------------------------------------------

void C16File::SetPalette( const C16_Palette& palette )
{
	// copy in the colors
	for (int idx = 0; idx < palette.iNumColors; ++idx)
	{
		m_pal.pColors[idx] = palette.pColors[idx];
	}

}

//------------------------------------------------------------------------------

void C16File::SetSCBs( const C16_SCB& scbs )
{
	// Free any existing SCBs
	if (m_scb.pSCB)
	{
		delete[] m_scb.pSCB;
		m_scb.pSCB = nullptr;
	}

	m_scb.iNumScanLines = scbs.iNumScanLines;

	if (scbs.iNumScanLines > 0 && scbs.pSCB != nullptr)
	{
		m_scb.pSCB = new uint8_t[ scbs.iNumScanLines ];
		memcpy(m_scb.pSCB, scbs.pSCB, scbs.iNumScanLines);
	}
}

//------------------------------------------------------------------------------
//
// Make a Copy of the image data (caller-supplied buffers are 1 byte per pixel)
//
void C16File::AddImages( const std::vector<unsigned char*>& pPixelMaps )
{
	int widthPixels = m_widthBytes * 2;
	int numPixels = widthPixels * m_heightPixels;

	for (int idx = 0; idx < pPixelMaps.size(); ++idx)
	{
		unsigned char* pPixels = new unsigned char[ numPixels ];
		memcpy(pPixels, pPixelMaps[ idx ], numPixels);
		m_pPixelMaps.push_back( pPixels );
	}
}

//------------------------------------------------------------------------------
// Nibble pack: two pixels per byte; even x -> high nibble, odd x -> low nibble.
//
void C16File::NibblePack(const unsigned char* pSrc, unsigned char* pDst,
						 int widthPixels, int heightPixels)
{
	int packedRowBytes = (widthPixels + 1) / 2;

	for (int y = 0; y < heightPixels; ++y)
	{
		const unsigned char* pSrcRow = pSrc + (y * widthPixels);
		unsigned char* pDstRow = pDst + (y * packedRowBytes);

		for (int x = 0; x < widthPixels; x += 2)
		{
			unsigned char hi = pSrcRow[ x ] & 0x0F;
			unsigned char lo = ((x + 1) < widthPixels) ? (pSrcRow[ x + 1 ] & 0x0F)
													   : 0;
			pDstRow[ x >> 1 ] = (unsigned char)((hi << 4) | lo);
		}
	}
}

//------------------------------------------------------------------------------
// Nibble unpack: inverse of NibblePack.
//
void C16File::NibbleUnpack(const unsigned char* pSrc, unsigned char* pDst,
						   int widthPixels, int heightPixels)
{
	int packedRowBytes = (widthPixels + 1) / 2;

	for (int y = 0; y < heightPixels; ++y)
	{
		const unsigned char* pSrcRow = pSrc + (y * packedRowBytes);
		unsigned char* pDstRow = pDst + (y * widthPixels);

		for (int x = 0; x < widthPixels; x += 2)
		{
			unsigned char b = pSrcRow[ x >> 1 ];
			pDstRow[ x ] = (b >> 4) & 0x0F;
			if ((x + 1) < widthPixels)
				pDstRow[ x + 1 ] = b & 0x0F;
		}
	}
}

//------------------------------------------------------------------------------
//
// If we're trying to save out a series of images, instead make a giant
// vertical film-strip
//
void C16File::CombinePixelMaps()
{
	int numFrames = (int)m_pPixelMaps.size();

	if (numFrames > 1)
	{
		int widthPixels = m_widthBytes * 2;
		int numPixelsPerFrame = widthPixels * m_heightPixels;
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
void C16File::SaveToFile(const char* pFilenamePath)
{
	// Handle Animation, by saving out a vertical film-strip
	CombinePixelMaps();

	// Actually, going to serialize to memory, then will save that to file
	std::vector<unsigned char> bytes;

	//--------------------------------------------------------------------------
	// Add the header
	bytes.resize( bytes.size() + sizeof(C16File_Header) );

	//$$JGA Rememeber, you have to set the pointer, before every access
	//$$JGA to the header data, because vector is going to change out
	//$$JGA memory addresses from underneath you
	C16File_Header* pHeader = (C16File_Header*)&bytes[0];

	pHeader->hi = 'I'; pHeader->h2 = '1'; pHeader->h5 = '6'; pHeader->h6 = 'I';

	pHeader->file_length = (unsigned int)bytes.size(); // get some valid data in there

	pHeader->version = 0x0000;
	pHeader->width  = m_widthBytes   & 0xFFFF;
	pHeader->height = m_heightPixels & 0xFFFF;
	pHeader->reserved = 0x0000;

	//--------------------------------------------------------------------------
	// Add a CLUT Chunk -- 2 bytes per packed BGRA color
	unsigned int clut_size = (m_pal.iNumColors * 2) + sizeof(C16File_CLUT);

	size_t clut_offset = bytes.size();

	size_t decompressed_clut_size = m_pal.iNumColors * 2;

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
		clut_size = (unsigned int) (compSize + sizeof(C16File_CLUT));
		// Add space for the CLUT
		bytes.resize( bytes.size() + clut_size );
		C16File_CLUT* pCLUT = (C16File_CLUT*) &bytes[ clut_offset ];
		pCLUT->c = 'C'; pCLUT->l = 'L'; pCLUT->u = 'U'; pCLUT->t = 'T';
		pCLUT->chunk_length = clut_size;
		pCLUT->num_colors = (unsigned short)(m_pal.iNumColors) | (unsigned short)0x8000; // signal compressed

		memcpy(&bytes[ clut_offset + sizeof(C16File_CLUT) ], pCompressedBuffer,
			   compSize);

	}
	else
	{
		// Save Decompressed
		// Add space for the CLUT
		bytes.resize( bytes.size() + clut_size );
		C16File_CLUT* pCLUT = (C16File_CLUT*) &bytes[ clut_offset ];

		pCLUT->c = 'C'; pCLUT->l = 'L'; pCLUT->u = 'U'; pCLUT->t = 'T';
		pCLUT->chunk_length = clut_size;
		pCLUT->num_colors = (unsigned short)(m_pal.iNumColors);

		// Packed colors are already in on-disk layout (16-bit little-endian BGRA)
		memcpy(&bytes[ clut_offset + sizeof(C16File_CLUT) ], m_pal.pColors,
			   decompressed_clut_size);
	}

	delete[] pCompressedBuffer;
	//--------------------------------------------------------------------------
	// Add a PIXL Chunk -- nibble-packed pixel data

	size_t pixl_offset = bytes.size();

	// Add space for the PIXL header;
	bytes.resize( bytes.size() + sizeof(C16File_PIXL) );
	C16File_PIXL* pPIXL = (C16File_PIXL*)&bytes[ pixl_offset ];

	pPIXL->p = 'P'; pPIXL->i = 'I'; pPIXL->x = 'X'; pPIXL->l = 'L';
	pPIXL->chunk_length = 0; // Temporary Chunk Size

	// Nibble-pack the pixel data first. Disk byte width per row = m_widthBytes.
	int packedRowBytes = m_widthBytes;
	int widthPixels    = m_widthBytes * 2;
	size_t decompressed_size = (size_t)packedRowBytes * (size_t)m_heightPixels;

	unsigned char* pPackedPixels = new unsigned char[ decompressed_size ];
	NibblePack(m_pPixelMaps[ 0 ], pPackedPixels, widthPixels, m_heightPixels);

	pPIXL->num_blobs = (short) (decompressed_size / 0x10000);

	// Need to add an extra blob, if we're not a multiple of 65536
	if (decompressed_size & 0xFFFF)
	{
		pPIXL->num_blobs+=1;
	}

	int num_blobs = pPIXL->num_blobs;

	// Work Buffer Guaranteed to be large enough
	unsigned char* pWorkBuffer = new unsigned char[ lzsa_get_max_compressed_size_inmem( 65536 ) ];
	// Compress the packed (nibble) data
	unsigned char *pSourceData = pPackedPixels;

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
			delete[] pPackedPixels;
			delete[] pWorkBuffer;
			exit(-1);
			return; // just stop
		}
	}

	delete[] pPackedPixels;
	delete[] pWorkBuffer;

	// Update the chunk length
	pPIXL = (C16File_PIXL*)&bytes[ pixl_offset ];
	pPIXL->chunk_length = (unsigned int) (bytes.size() - pixl_offset);

	//--------------------------------------------------------------------------
	// Add an SCBs Chunk -- one Scanline Control Byte per scanline
	// (Apple IIgs convention: bit 7 = 320/640 mode, bits 3-0 = palette index).
	// Mirrors the CLUT compression scheme: high bit of num_scbs = compressed flag.
	if ((m_scb.iNumScanLines > 0) && (m_scb.pSCB != nullptr))
	{
		size_t scb_offset = bytes.size();
		size_t decompressed_scb_size = (size_t)m_scb.iNumScanLines;

		unsigned char* pScbCompBuffer = new unsigned char[
			lzsa_get_max_compressed_size_inmem( decompressed_scb_size ) ];

		size_t scbCompSize = lzsa_compress_inmem(
			m_scb.pSCB,                                                            // input
			pScbCompBuffer,                                                        // output
			decompressed_scb_size,                                                 // input size
			lzsa_get_max_compressed_size_inmem( decompressed_scb_size ),           // max output buffer size
			LZSA_FLAG_FAVOR_RATIO | LZSA_FLAG_RAW_BLOCK,
			0,                                                                     // minmatchsize (0 better for ratio)
			2                                                                      // Format Version
		);

		unsigned int scb_chunk_size;
		if ((scbCompSize > 0) && (scbCompSize < decompressed_scb_size))
		{
			// Save compressed
			scb_chunk_size = (unsigned int)(scbCompSize + sizeof(C16File_SCBs));
			bytes.resize( bytes.size() + scb_chunk_size );
			C16File_SCBs* pSCBs = (C16File_SCBs*)&bytes[ scb_offset ];
			pSCBs->S = 'S'; pSCBs->c = 'C'; pSCBs->b = 'B'; pSCBs->s = 's';
			pSCBs->chunk_length = scb_chunk_size;
			pSCBs->num_scbs = (unsigned short)(m_scb.iNumScanLines) | (unsigned short)0x8000;

			memcpy(&bytes[ scb_offset + sizeof(C16File_SCBs) ], pScbCompBuffer,
				   scbCompSize);
		}
		else
		{
			// Save uncompressed
			scb_chunk_size = (unsigned int)(decompressed_scb_size + sizeof(C16File_SCBs));
			bytes.resize( bytes.size() + scb_chunk_size );
			C16File_SCBs* pSCBs = (C16File_SCBs*)&bytes[ scb_offset ];
			pSCBs->S = 'S'; pSCBs->c = 'C'; pSCBs->b = 'B'; pSCBs->s = 's';
			pSCBs->chunk_length = scb_chunk_size;
			pSCBs->num_scbs = (unsigned short)(m_scb.iNumScanLines);

			memcpy(&bytes[ scb_offset + sizeof(C16File_SCBs) ], m_scb.pSCB,
				   decompressed_scb_size);
		}

		delete[] pScbCompBuffer;
	}

	//--------------------------------------------------------------------------
	// Update the header
	pHeader = (C16File_Header*)&bytes[0]; // Required
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

void C16File::LoadFromFile(const char* pFilePath)
{
	// Free any existing memory
	if (m_pal.pColors)
	{
		delete[] m_pal.pColors;
		m_pal.pColors = nullptr;
	}
	if (m_scb.pSCB)
	{
		delete[] m_scb.pSCB;
		m_scb.pSCB = nullptr;
	}
	m_scb.iNumScanLines = 0;
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
		C16File_Header* pHeader = (C16File_Header*) &bytes[0];

		// Early out if things don't look right
		if (!pHeader->IsValid((unsigned int)bytes.size()))
			return;

		m_widthBytes   = pHeader->width;
		m_heightPixels = pHeader->height;

		// Go ahead and allocate the bitmap (1 byte per pixel after unpack)
		int widthPixels = m_widthBytes * 2;
		size_t frameSize = (size_t)widthPixels * (size_t)m_heightPixels;

		// Allocate a Frame
		unsigned char* pFrame = new unsigned char[ frameSize ];
		// Save it in the list
		m_pPixelMaps.push_back(pFrame);

		//----------------------------------------------------------------------
		// Process Chunks as we encounter them
		file_offset += sizeof(C16File_Header);

		// While we're not at the end of the file
		while (file_offset < bytes.size())
		{
			// This is pretty dumb, just get it done
			// These are the types I understand
			// every chunk is supposed to contain a value chunk_length
			// at offset +4, so that we can ignore ones we don't understand
			C16File_CLUT* pCLUT = (C16File_CLUT*)&bytes[ file_offset ];
			C16File_PIXL* pPIXL = (C16File_PIXL*)&bytes[ file_offset ];
			C16File_SCBs* pSCBs = (C16File_SCBs*)&bytes[ file_offset ];
			C16File_CHUNK* pCHUNK = (C16File_CHUNK*)&bytes[ file_offset ];

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
			else if (pSCBs->IsValid())
			{
				// We have an SCBs chunk
				UnpackSCBs(pSCBs);
			}

			file_offset += pCHUNK->chunk_length;
		}
	}
}

//------------------------------------------------------------------------------
//
//  Move data out of the CLUT block into the unpacked class structure
//
void C16File::UnpackClut(C16File_CLUT* pCLUT)
{
	int numColors = pCLUT->num_colors & 0x7FFF;

	// 2 bytes per packed BGRA color
	m_pal.iNumColors = numColors;

	unsigned char* pPacked = ((unsigned char*) pCLUT) + sizeof(C16File_CLUT);

	m_pal.pColors = new C16_Color[ numColors ];

	if (pCLUT->num_colors & 0x8000)
	{
		// data is compressed
		int version = 2; // format version;
		lzsa_decompress_inmem(pPacked, 		   // Compressed Data
		  					  (unsigned char *)m_pal.pColors,   // Target uncompressed data
							  pCLUT->chunk_length-sizeof(C16File_CLUT),  // compressed size in bytes
							  numColors * sizeof(C16_Color),
							  LZSA_FLAG_RAW_BLOCK,
							  &version);


	}
	else
	{
		memcpy(m_pal.pColors, pPacked, numColors * sizeof(C16_Color));
	}
}

//------------------------------------------------------------------------------
//
// Unpack the pixel bitmap, that's been weirdly packed into 64KB chunks
// to make it easier to deal with on 65816, then nibble-unpack to 1 byte/pixel.
//
void C16File::UnpackPixel(C16File_PIXL* pPIXL)
{
	int num_blobs = pPIXL->num_blobs;

	unsigned char *pData = ((unsigned char*)pPIXL) + sizeof(C16File_PIXL);

	int packedRowBytes = m_widthBytes;
	int widthPixels    = m_widthBytes * 2;
	size_t packedSize = (size_t)packedRowBytes * (size_t)m_heightPixels;

	// Decompress the nibble-packed bytes into a temp buffer first.
	unsigned char* pPackedBuffer = new unsigned char[ packedSize ];
	unsigned char* pPackedCursor = pPackedBuffer;
	size_t bufferSize = packedSize;

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
				pPackedCursor[ idx ] = pData[ idx ];
			}

			bufferSize -= compressedSize;

		}
		else
		{
			int version = 2; // format version;
			size_t decompressedSize = lzsa_decompress_inmem(pData, 		   // Compressed Data
															pPackedCursor, // Target uncompressed data
															compressedSize, // compressed size in bytes
															bufferSize,
															LZSA_FLAG_RAW_BLOCK,
															&version);

			bufferSize -= decompressedSize;

		}

		pData += compressedSize;
		pPackedCursor += 0x10000;
	}

	// Now nibble-unpack into the per-pixel target buffer
	NibbleUnpack(pPackedBuffer, m_pPixelMaps[ 0 ], widthPixels, m_heightPixels);

	delete[] pPackedBuffer;
}

//------------------------------------------------------------------------------
//
// Move data out of the SCBs block into the unpacked class structure.
// Mirrors UnpackClut: high bit of num_scbs == compressed flag.
//
void C16File::UnpackSCBs(C16File_SCBs* pSCBs)
{
	int numScanLines = pSCBs->num_scbs & 0x7FFF;

	// Free any prior SCBs (defensive; LoadFromFile also clears upfront).
	if (m_scb.pSCB)
	{
		delete[] m_scb.pSCB;
		m_scb.pSCB = nullptr;
	}

	m_scb.iNumScanLines = numScanLines;

	if (numScanLines <= 0)
		return;

	unsigned char* pPacked = ((unsigned char*) pSCBs) + sizeof(C16File_SCBs);

	m_scb.pSCB = new uint8_t[ numScanLines ];

	if (pSCBs->num_scbs & 0x8000)
	{
		// data is compressed
		int version = 2; // format version
		lzsa_decompress_inmem(pPacked,                                       // compressed data
							  (unsigned char *)m_scb.pSCB,                   // target uncompressed data
							  pSCBs->chunk_length - sizeof(C16File_SCBs),    // compressed size in bytes
							  numScanLines,
							  LZSA_FLAG_RAW_BLOCK,
							  &version);
	}
	else
	{
		memcpy(m_scb.pSCB, pPacked, numScanLines);
	}
}

//------------------------------------------------------------------------------
