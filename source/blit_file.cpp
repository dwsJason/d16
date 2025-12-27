//
// C++ Encoder
//

#include "blit_file.h"

#include <stdio.h>

//------------------------------------------------------------------------------

BLITFile::BLITFile(int iWidthPixels, int iHeightPixels, int iFrameSizeBytes )
	: m_widthPixels(iWidthPixels)
	, m_heightPixels(iHeightPixels)
	, m_frameSize( iFrameSizeBytes )
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

void GSLAFile::LoadFromFile(const char* pFilePath)
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
		GSLA_Header* pHeader = (GSLA_Header*) &bytes[0];

		// Early out if things don't look right
		if (!pHeader->IsValid((unsigned int)bytes.size()))
			return;

		// Size in bytes for each frame in this animation
		m_frameSize = pHeader->frame_size;

		// pre-allocate all the frames
		for (unsigned int idx = 0; idx < pHeader->frame_count; ++idx)
		{
			m_pC1PixelMaps.push_back(new unsigned char[ m_frameSize ]);
		}

		//----------------------------------------------------------------------
		// Process Chunks as we encounter them
		file_offset += sizeof(GSLA_Header);

		// While we're not at the end of the file
		while (file_offset < bytes.size())
		{
			// This is pretty dumb, just get it done
			// These are the types I understand
			// every chunk is supposed to contain a value chunk_length
			// at offset +4, so that we can ignore ones we don't understand
			GSLA_INIT* pINIT = (GSLA_INIT*)&bytes[ file_offset ];
			GSLA_ANIM* pANIM = (GSLA_ANIM*)&bytes[ file_offset ];
			GSLA_CHUNK* pCHUNK = (GSLA_CHUNK*)&bytes[ file_offset ];

			if (pINIT->IsValid())
			{
				// We have an initial frame chunk
				UnpackInitialFrame(pINIT, pHeader);
			}
			else if (pANIM->IsValid())
			{
				// We have a packed animation frames chunk
				UnpackAnimation(pANIM, pHeader);
			}

			file_offset += pCHUNK->chunk_length;

		}
	}
}

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//
// Unpack the initial frame, that's been packed with an empty initial dictionary
// So every byte of the buffer will be written out (no skip opcodes)
//
void GSLAFile::UnpackInitialFrame(GSLA_INIT* pINIT, GSLA_Header* pHeader)
{
	unsigned char* pData = ((unsigned char*)pINIT) + sizeof(GSLA_INIT);

	unsigned char* pTargetBuffer = m_pC1PixelMaps[ 0 ]; // Data needs to be pre allocated

	DecompressFrame(pTargetBuffer, pData, (unsigned char*)pHeader);
}

//------------------------------------------------------------------------------
//
// Unpack the animation frame, assuming that the initial frame already exists
//
void GSLAFile::UnpackAnimation(GSLA_ANIM* pANIM, GSLA_Header* pHeader)
{
	unsigned char* pData = ((unsigned char*)pANIM) + sizeof(GSLA_ANIM);

	unsigned char *pCanvas = new unsigned char[m_frameSize];

	// Initialize the Canvas with the first frame
	memcpy(pCanvas, m_pC1PixelMaps[0], m_frameSize);

	for (int idx = 1; idx < m_pC1PixelMaps.size(); ++idx)
	{
		// Apply Changes to the Canvas
		pData += DecompressFrame(pCanvas, pData, (unsigned char*) pHeader);

		// Capture the Canvas
		memcpy(m_pC1PixelMaps[idx], pCanvas, m_frameSize);
	}
}

//------------------------------------------------------------------------------
//
// Append a copy of raw image data into the class
//
void GSLAFile::AddImages( const std::vector<unsigned char*>& pFrameBytes )
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
void GSLAFile::SaveToFile(const char* pFilenamePath)
{
	// We're not going to even try encoding an empty file
	if (m_pC1PixelMaps.size() < 1)
	{
		return;
	}

	// serialize to memory, then save that to a file
	std::vector<unsigned char> bytes;

	//--------------------------------------------------------------------------
	// Add the header
	bytes.resize( bytes.size() + sizeof(GSLA_Header) );

	//$$JGA Remember, you have to set the pointer, before every access
	//$$JGA to the header data, because vector is going to change out
	//$$JGA memory addresses from underneath you
	GSLA_Header* pHeader = (GSLA_Header*)&bytes[0];

	pHeader->G = 'G'; pHeader->S = 'S'; pHeader->L = 'L'; pHeader->A = 'A';

	pHeader->file_length = 0;  // Temp File Length

	pHeader->version = 0x8000; // Version 0, with a Ring/Loop Frame at the end

	pHeader->width = (unsigned short)m_widthPixels >> 1;
	pHeader->height = (unsigned short)m_heightPixels;

	pHeader->frame_size = (unsigned short)m_frameSize;

	pHeader->frame_count = (unsigned int)m_pC1PixelMaps.size() + 1; // + 1 for the ring frame

	//--------------------------------------------------------------------------
	// Add the INITial frame chunk
	//
	// If there's only an initial frame, I guess this becomes a picture
	//

	size_t init_offset = bytes.size();

	// Add space for the INIT header
	bytes.resize( bytes.size() + sizeof(GSLA_INIT) );
	GSLA_INIT* pINIT = (GSLA_INIT*) &bytes[ init_offset ];

	pINIT->I = 'I'; pINIT->N = 'N'; pINIT->i = 'I'; pINIT->T = 'T';
	pINIT->chunk_length = 0; // temp chunk size

	printf("Save Initial Frame\n");

	// Need a place to put compressed data, in theory it could be bigger
	// than the original data, I think if that happens, the image was probably
	// designed to break this, anyway, give double theoretical max
	unsigned char* pWorkBuffer = new unsigned char[ m_frameSize * 2 ];

	unsigned char* pInitialFrame = m_pC1PixelMaps[ 0 ];

	// We're not worried about bank wrap on the first frame, and we don't have a pre-populated
	// dictionary - Also use the best compression we can get here
	int compressedSize = Old_LZB_Compress(pWorkBuffer, pInitialFrame, m_frameSize);

	printf("frameSize = %d\n", compressedSize);

	for (int compressedIndex = 0; compressedIndex < compressedSize; ++compressedIndex)
	{
		bytes.push_back(pWorkBuffer[ compressedIndex ]);
	}

	// Insert EOF/ End of Animation Done opcode
	bytes.push_back( 0x06 );
	bytes.push_back( 0x00 );


	// Reset pointer to the pINIT (as the baggage may have shifted)
	pINIT = (GSLA_INIT*) &bytes[ init_offset ];
	pINIT->chunk_length = (unsigned int) (bytes.size() - init_offset);

	//--------------------------------------------------------------------------
	// Add the ANIMation frames chunk
	//
	// We always add this, because we always add a Ring/Loop frame, we always
	// end up with at least 2 frames
	//

	size_t anim_offset = bytes.size();

	// Add Space for the ANIM Header
	bytes.resize( bytes.size() + sizeof(GSLA_ANIM) );
	GSLA_ANIM* pANIM = (GSLA_ANIM*) &bytes[ anim_offset ];

	pANIM->A = 'A'; pANIM->N = 'N'; pANIM->I ='I'; pANIM->M = 'M';
	pANIM->chunk_length = 0; // temporary chunk size

	// Initialize the Canvas with the initial frame (we alread exported this)
	unsigned char *pCanvas = new unsigned char[ m_frameSize ];
	memcpy(pCanvas, m_pC1PixelMaps[0], m_frameSize);

	// Let's encode some frames buddy
	for (int frameIndex = 1; frameIndex < m_pC1PixelMaps.size(); ++frameIndex)
	{
		printf("Save Frame %d\n", frameIndex+1);

		// I don't want random data in the bank gaps, so initialize this
		// buffer with zero
		//memset(pWorkBuffer, 0xEA, m_frameSize * 2);

		int frameSize = LZBA_Compress(pWorkBuffer, m_pC1PixelMaps[ frameIndex ],
									  m_frameSize, pWorkBuffer-bytes.size(),
									  pCanvas, m_frameSize );

		//int canvasDiff = memcmp(pCanvas, m_pC1PixelMaps[ frameIndex], m_frameSize);
		//if (canvasDiff)
		//{
		//	printf("Canvas is not correct - %d\n", canvasDiff);
		//}
		printf("frameSize = %d\n", frameSize);


		for (int index = 0; index < frameSize; ++index)
		{
			bytes.push_back(pWorkBuffer[ index ]);
		}
	}

	// Add the RING Frame
	//memset(pWorkBuffer, 0xAB, m_frameSize * 2);

	printf("Save Ring Frame\n");

	int ringSize = LZBA_Compress(pWorkBuffer, m_pC1PixelMaps[ 0 ],
								  m_frameSize, pWorkBuffer-bytes.size(),
								  pCanvas, m_frameSize  );

	printf("Ring Size %d\n", ringSize);

	for (int ringIndex = 0; ringIndex < ringSize; ++ringIndex)
	{
		bytes.push_back(pWorkBuffer[ ringIndex ]);
	}

	delete[] pCanvas; pCanvas = nullptr;

	// Insert End of file/ End of Animation Done opcode
	// -- There has to be room for this, or there wouldn't be room to insert
	// -- a source bank skip opcode
	bytes.push_back( 0x06 );
	bytes.push_back( 0x00 );

	// Update the chunk length
	pANIM = (GSLA_ANIM*)&bytes[ anim_offset ];
	pANIM->chunk_length = (unsigned int) (bytes.size() - anim_offset);

	// Update the header
	pHeader = (GSLA_Header*)&bytes[0]; // Required
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
// Std C memcpy seems to be stopping the copy from happening, when I overlap
// the buffer to get a pattern run copy (overlapped buffers)
//
static void my_memcpy(unsigned char* pDest, unsigned char* pSrc, int length)
{
	while (length-- > 0)
	{
		*pDest++ = *pSrc++;
	}
}

//------------------------------------------------------------------------------
//
//  pTarget is the Target Frame Buffer
//  pData is the source data for a Frame
// 
//  pDataBaseAddress, is the base address wheret the animation file was loaded
//  this is used so we can properly interpret bank-skip opcodes (data is broken
//  into 64K chunks for the IIgs/65816)
//  
//  returns the number of bytes that have been processed in the pData
//
int GSLAFile::DecompressFrame(unsigned char* pTarget, unsigned char* pData, unsigned char* pDataBaseAddress)
{
	unsigned char *pDataStart = pData;

	int cursorPosition = 0;
	unsigned short opcode;

	bool bDoWork = true;

	while (bDoWork)
	{
		opcode  = pData[0];
		opcode |= (((unsigned short)pData[1])<<8);

		if (opcode & 0x8000)
		{
			if (opcode & 0x0001)
			{
				// Cursor Skip Forward
				opcode = (opcode>>1) & 0x3FFF;
				cursorPosition += (opcode+1);
				pData+=2;
			}
			else
			{
				// Dictionary Copy
				unsigned short dictionaryPosition = pData[2];
				dictionaryPosition |= (((unsigned short)pData[3])<<8);

				dictionaryPosition -= 0x2000;	// it's like this to to help the
											    // GS decode it quicker

				unsigned short length = ((opcode>>1) & 0x3FFF)+1;

				my_memcpy(pTarget + cursorPosition, pTarget + dictionaryPosition, (int) length );

				pData += 4;
				cursorPosition += length;
			}
		}
		else
		{
			if (opcode & 0x0001)
			{
				// Literal Copy Bytes
				pData += 2;
				unsigned short length = ((opcode>>1) & 0x3FFF)+1;

				my_memcpy(pTarget + cursorPosition, pData, (int) length);

				pData += length;
				cursorPosition += length;
			}
			else
			{
				opcode = ((opcode>>1)) & 3;

				switch (opcode)
				{
				case 0: // Source bank Skip
					{
						int offset = (int)(pData - pDataBaseAddress);

						offset &= 0xFFFF0000;
						offset += 0x00010000;

						pData = pDataBaseAddress + offset;
					}
					break;
				case 1: // End of frame
					pData+=2;
					bDoWork = false;
					break;

				case 3: // End of Animation
					// Intentionally, leave cursor alone here
					bDoWork = false;
					break;

				default:
					// Reserved / Illegal
					bDoWork = false;
					break;
				}

			}
		}


	}

	return (int)(pData - pDataStart);
}

//------------------------------------------------------------------------------

