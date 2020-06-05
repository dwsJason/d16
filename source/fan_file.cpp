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

	// Update the chunk length
	pINIT = (FanFile_INIT*)&bytes[ init_offset ];
	pINIT->chunk_length = (unsigned int) (bytes.size() - init_offset);

	//--------------------------------------------------------------------------
	// Add a FRAMes Chunk 

	if (m_pPixelMaps.size() > 1)
	{
		delete[] pWorkBuffer;
		pWorkBuffer = new char[ decompressed_size * 2 ]; // this is pretty dumb

		size_t fram_offset = bytes.size();

		// Add space for the INIT header;
		bytes.resize( bytes.size() + sizeof(FanFile_FRAM) );
		FanFile_FRAM* pFRAM = (FanFile_FRAM*)&bytes[ fram_offset ];

		pFRAM->f = 'F'; pFRAM->r = 'R'; pFRAM->a = 'A'; pFRAM->m = 'M';
		pFRAM->chunk_length = 0; // Temporary Chunk Size

		// Initializae Canvas with the initial frame
		unsigned char* pCanvas = new unsigned char[ decompressed_size ];
		memcpy(pCanvas, pSourceData, decompressed_size);

		for (int index = 1; index < m_pPixelMaps.size(); ++index)
		{
			unsigned char *pFrame = m_pPixelMaps[ index ];

			int compSize = EncodeFrame(pCanvas, pFrame, (unsigned char*)pWorkBuffer, decompressed_size);

			if (compSize > 0)
			{
				// is this fast?  Probably not
				for (int compressedIndex = 0; compressedIndex < compSize; ++compressedIndex)
				{
					bytes.push_back((unsigned char)pWorkBuffer[ compressedIndex ]);
				}
			}
		}

		delete[] pCanvas;

		// END OF FILE / END OF CHUNK
		bytes.push_back(0xFF);
		bytes.push_back(0xFF);

		// Update the chunk length
		pFRAM = (FanFile_FRAM*)&bytes[ init_offset ];
		pFRAM->chunk_length = (unsigned int) (bytes.size() - fram_offset);
	}

	//--------------------------------------------------------------------------
	// Update the header
	pHeader = (FanFile_Header*)&bytes[0]; // Required
	pHeader->file_length = (unsigned int)bytes.size(); // get some valid data in there

	// Try not to leak memory, even though we probably do
	delete[] pWorkBuffer;

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
//
// A Frame encoded with a 16 bit code word, followed by some kind of action
// 
// Each Frame starts with the assumption that there's a cursor at offset 0
// in the frame
//
// %10xx_xxxx_xxxx_xxxx - Copy Bytes  (1-16384) to follow the word, each byte
// 						  moves the cursor forwarc
// %01xx_xxxx_xxxx_xxxx - Skip Bytes  (1-16384) move the "cursor" forward
// %00xx_xxxx_xxxx_xxxx - LZ4 Decompress Bytes / uncompressed size of bytes to
//					      to follow (1-16384)
// %1100_0000_xxxx_xxxx - Realtime Color Swap
//						  xxx_xxxx number of colors, 1 byte follows, with the
//                        the color index to start copying onto 0-255, followed
//                        by the colors
//                        Colors in B G R 255 format follow (so they can be
//                        straight copied)
// 
// %1111_1111_1111_1111 - End of Chunk / End of File (Loop here?)
//
//
//  pCanvas is the reference Canvas
//  pFrame  is the new frame
//  pWorkBuffer is where the encoded data goes
//  bufferSize is the size in bytes of the uncompressed data
//
//  Return encoded size
//
int FanFile::EncodeFrame(unsigned char* pCanvas, unsigned char* pFrame, unsigned char* pWorkBuffer, size_t bufferSize )
{
	int resultSize = 0;
	int sourceCursor = 0;
	// There are opportunities to make this better than it is
	// this is a what I call, "dumb" implementation

	// Start by looking for bytes to skip (this is by far the most efficient
	// thing we can do while encoding

	// The easiest thing I can do to guarantee no issues with banks changing
	// is to only analyze the data in 64KB chunks
	int num_blobs = (int)(bufferSize / 0x10000);
	if (bufferSize & 0xFFFF)
	{
		num_blobs+=1;
	}

	for (int idx = 0; idx < num_blobs; ++idx)
	{
		size_t sourceOffset = 0x10000 * idx;
		int decompressedChunkSize = (int)(bufferSize - sourceOffset);
		if (decompressedChunkSize > 0x10000)
		{
			decompressedChunkSize = 0x10000;
		}

		// Now we know we're looking at data that is 64K or less
		unsigned char *pSrcCanvas = &pCanvas[ sourceOffset ];
		unsigned char *pSrcFrame  = &pFrame[ sourceOffset ];

		int bridge_run_size = 0;	// how many bytes are we bridging that could be "skipped"

		for (int analyzeIndex = 0; analyzeIndex < decompressedChunkSize;/*++analyzeIndex*/)
		{
			if (!bridge_run_size && pSrcCanvas[ analyzeIndex ] == pFrame[ analyzeIndex ])
			{
				// We might be skipping some stuff here, we only want to skip if the
				// skip size is more than 2
				int skip_run_size = 1;

				while ((analyzeIndex+skip_run_size) < decompressedChunkSize)
				{
					if (pSrcCanvas[analyzeIndex + skip_run_size] == pSrcFrame[analyzeIndex + skip_run_size])
					{
						skip_run_size++;
					}
					else
						break;
				}

				// Look at the skip_run_size, to see what to do next, we need
				// it to be 2 to break even, but if we end because we're out
				// of data, then, whatever, it is what it is

				if ((skip_run_size > 2) || ((analyzeIndex+skip_run_size) == decompressedChunkSize))
				{
					// Let's do it

					// This is going to cause is to re-analyze some of the image
					if (skip_run_size > 16384)
					{
						skip_run_size = 16384;
					}

					//if (skip_run_size <= 16384)
					{
						// Perfect
						unsigned short command = 0x4000 | ((unsigned short)skip_run_size - 1);
						*pWorkBuffer++ = (unsigned char)(command & 0xFF);
						*pWorkBuffer++ = (unsigned char)((command>>8) & 0xFF);
						sourceCursor += skip_run_size;
						analyzeIndex += skip_run_size;
					}
				}
				else
				{
					bridge_run_size++;
				}
			}
			else
			{
				// Bytes differ, due to the encoding, we can eat a series of
				// bytes that don't differ, and actually end up more efficient
				// thank encoding a skip command (more efficient means faster
				// executing code, and smaller total size, so yes both)
				//
				int copy_run_size = 1;

				while ((analyzeIndex+copy_run_size) < decompressedChunkSize)
				{
					if (pSrcCanvas[ analyzeIndex+copy_run_size ] != pSrcFrame[ analyzeIndex+copy_run_size ])
					{
						copy_run_size++;
						bridge_run_size = 0;
					}
					else
					{
						copy_run_size++;
						bridge_run_size++;
						if (bridge_run_size > 4)
						{
							break;
						}
					}
				}

				// if we hit the end of the data / bank, then we just keep what
				// we have, otherwise, reduce the size of the copy, by the size
				// of the bridge

				if ((analyzeIndex+copy_run_size) != decompressedChunkSize)
				{
					copy_run_size -= bridge_run_size;
				}

				bridge_run_size = 0;

				if (copy_run_size > 16384)
				{
					// Again, is going to cause some data to be analyzed twice
					copy_run_size = 16384;
				}

				if (copy_run_size > 256)
				{
					// LZ4 Compress the chunk to be copied
				}

				{
					unsigned short command = 0x8000 | ((unsigned short)copy_run_size - 1);
					*pWorkBuffer++ = (unsigned char)(command & 0xFF);
					*pWorkBuffer++ = (unsigned char)((command>>8) & 0xFF);

					for (int copyIndex = 0; copyIndex <= copy_run_size; ++copyIndex)
					{
						*pWorkBuffer++ = pFrame[ analyzeIndex + copyIndex ];
					}

					sourceCursor += copy_run_size;
					analyzeIndex += copy_run_size;
				}

			}
		}
	}

	// if copy is large, then look at doing LZ4 compress


	// At ending, make sure pCanvas matches pFrame
	memcpy(pCanvas, pFrame, bufferSize);


	return resultSize;
}

//------------------------------------------------------------------------------


