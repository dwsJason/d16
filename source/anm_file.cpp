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
#include <assert.h>

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
			unsigned char* FirstLPage = &bytes[ sizeof(ANM_Header) ];
			// Looks Valid

			// I don't support palette animation, just just grab
			// the palette from the header

			// This is another format that applies delta changes
			// to a reference buffer
			unsigned char* pCanvas = new unsigned char[ 320 * 200 ];
			memset(pCanvas, 0, 320*200);

			unsigned char* pData = FindRecord(pHeader, 0);

			DecodeFrame(pCanvas, pData);

			unsigned char* pFrame = new unsigned char[320 * 200];
			memcpy(pFrame, pCanvas, 320 * 200 );


			// Now grab first frame of animation

			// loop through and grab the rest of the frames
			for (int frameNo = 1; frameNo < (int)pHeader->nFrames; ++frameNo)
			{
				unsigned char* pFramePointer = FindRecord(pHeader, frameNo);

				// 66 magic frame IDnum
				assert(66 == pFramePointer[0]);

				assert(0 == pFramePointer[1]); // if we hit this, we need to
										       // add support for the extrabytes

				// Assume no extra bytes field
				pFramePointer += 2;

				// Now we're pointed at compressed data, probably

			}
		}
	}

}

//------------------------------------------------------------------------------
//
// RunSkipDump, not documented in animfile.txt, so that's helpful
// 
//
void AnmFile::DecodeFrame(unsigned char* pDest, unsigned char* pSource)
{
}

//------------------------------------------------------------------------------
// Return pointer to the requested frame Number
//
unsigned char* AnmFile::FindRecord(ANM_Header* pHeader, int frameNo)
{
	unsigned char* pResult = nullptr;

	if (frameNo < (int)pHeader->nFrames)
	{
		// Find the large page with the frame we want
		for (int largePageIndex = 0; largePageIndex < pHeader->nLps; ++largePageIndex)
		{
			ANM_LargePage* pPageRecord = &pHeader->LParay[ largePageIndex ];

			if (pPageRecord->baseRecord <= frameNo)
			{
				if (frameNo < (pPageRecord->baseRecord + pPageRecord->nRecords))
				{
					pResult  = (unsigned char *)pHeader;
					pResult += sizeof(ANM_Header);
					pResult += (0x10000 * largePageIndex);

					ANM_LargePage* pLargePage = (ANM_LargePage*) pResult;

					assert(pLargePage->baseRecord == pPageRecord->baseRecord);
					assert(pLargePage->nBytes     == pPageRecord->nBytes);
					assert(pLargePage->nRecords   == pPageRecord->nRecords);

					pResult += sizeof(ANM_LargePage);

					u16 bytesContinued = pResult[0];
					bytesContinued    |= (((u16)pResult[1])<<8);
					pResult += sizeof(u16);
					assert(0 == bytesContinued);

					// Pointer to Record sizes Array
					u16* pRecordSizes = (u16*)pResult;

					pResult += (sizeof(u16) * pLargePage->nRecords);

					// Bring us to the first record in the page, but which
					// record do we want in the page?
					int localRecordIndex = frameNo - pLargePage->baseRecord;

					for (int idx = 0; idx < localRecordIndex; ++idx)
					{
						pResult += pRecordSizes[ idx ];
					}

					break;
				}
			}
		}
	}

	return pResult;
}
//------------------------------------------------------------------------------

