//
// C++ Decoder
// For ANM, Deluxe Animation File Format
// 
//  This link should be good, at least until Jason Scott passes away
//  after that the way back machine should have a copy
// 
//  http://www.textfiles.com/programming/FORMATS/animfile.txt
//
#ifndef ANM_FILE_H
#define ANM_FILE_H

#include "bctypes.h"
#include <vector>

//------------------------------------------------------------------------------

#pragma pack(push, 1)

typedef struct ANM_Color
{
    unsigned char r;
	unsigned char g;
	unsigned char b;
	unsigned char a;
} ANM_Color;

//------------------------------------------------------------------------------

typedef struct ANM_Range
{
	u16 count;
	u16 rate;
	u16 flags;
	u8 low,high; // bounds of range
} ANM_Range;

typedef struct ANM_LargePage
{
	u16 baseRecord; // number of the first record in this large page
	u16 nRecords;	// number of records in lp
				    // bit 15 of "nRecords" == "has continuation from previous lp".
					// bit 14 of "nRecords" == "final record continues on next lp".
	u16 nBytes;		// Total number of bytes of contents, excluding header

} ANM_LargePage;

typedef struct ANM_Header
{
	u8 l,p,f,space ;  // id, "LPF "
	u16 maxLps; 	  // Max Large Pages
	u16 nLps;		  // Large Pages in this file
	u32 nRecords;	  // records in this file, 65534 limit + 1 for last-to-first
					  // delta for looping animation
	u16 maxRecsPerLp; // records permitted in a loop, 256 FOR NOW
	u16 lpfTableOffset; // Absolute seek position of lpfTable (1280 for now)
			          // lpt table is an array of 256 large page structures
					  // that is used to facilitate finding records in an anim
					  // file without having to seek through all of the Large
					  // Pages to find which one a specific record lives in.
	u8 a,n,i,m;		  // contentType "ANIM"

	u16 width;		  // width in pixels
	u16 height;	      // height in pixels
	u8  variant; 	  // 0 == ANIM
	u8  version;      // 0==frame rate is multiple of 18 cycles/sec.
					  // 1==frame rate is multiple of 70 cycles/sec.
	u8	hasLastDelta; // 1==Last record is a delta from last-to-first frame.
	u8	lastDeltaValid; // 0==The last-to-first delta (if present) hasn't been
						// updated to match the current first&last frames, so it
					    // should be ignored
	u8	pixelType;	  // 0 == 256 color.
	u8	CompressionType; // 1==RunSkipDump (only option)
	u8	otherRecsPerFrm; // 0 FOR NOW
	u8	bitmaptype;	 	// 1==320x200, 256 color. (only one for now)

	u8	recordTypes[32]; // not yet implemented

	u32 nFrames;	// Number of frames, including last-to-first delta when present

	u16 framesPerSecond; // NUmber of frames to play per second

	u16 pad2[29];	// 58 bytes of filler to round up to 128 bytes total

	ANM_Range cycles[16]; // 128 bytes, color cycling structures

	u32 palette[256];  // RGB and unused per color

//	Following the palette is an array of structures that are copies of the
//	large page headers.  This array is loaded and used to find which large page
//	a given frame can be found in.

	ANM_LargePage LParay[ 256 ]; //Copies of all the Large Page Structures in
						// the anim file.  Each Large Page structure is 6 bytes
						// long so the total length of this table is 1536 bytes.
						// Even if the file only contains 1 large page there are
						// still 256 entris of 6 byes each.


//------------------------------------------------------------------------------
// If you're doing C, just get rid of these methods
	bool IsValid()
	{
		if ((l!='L')||(p!='P')||(f!='F')||(space!=' '))
			return false;				// signature is not right

		if ((a!='A')||(n!='N')||(i!='I')||(m!='M'))
			return false;				// signature is not right

		if ((320 != width)||(200 != height))
			return false;

		if (0 != variant)
			return false;

		if (0 != pixelType)
			return false;

		if (1 != CompressionType)
			return false;

		if (0 != otherRecsPerFrm)
			return false;

		if (1 != bitmaptype)
			return false;

		if (1280 != lpfTableOffset)
			return false;

		return true;
	}

} ANM_Header;

#pragma pack(pop)

//------------------------------------------------------------------------------
typedef struct ANM_Palette
{
    int iNumColors;
    ANM_Color* pColors;

} ANM_Palette;
//------------------------------------------------------------------------------

class AnmFile
{
public:
	// Load an Anm File
	AnmFile(const char *pFilePath);

	~AnmFile();

	// Retrieval
	void LoadFromFile(const char* pFilePath);
	int GetFrameCount() { return (int)m_pPixelMaps.size(); }
	int GetWidth()  { return m_widthPixels; }
	int GetHeight() { return m_heightPixels; }

	const ANM_Palette& GetPalette() { return m_pal; }
	const std::vector<unsigned char*> GetPixelMaps() { return m_pPixelMaps; }

private:

	void DecodeFrame(unsigned char* pDest, unsigned char* pSource);
	unsigned char* FindRecord(ANM_Header* pHeader, int frameNo);

	int m_widthPixels;		// Width of image in pixels
	int m_heightPixels;		// Height of image in pixels
	int m_numColors;		// number of colors in the initial CLUT

	ANM_Palette m_pal;

	std::vector<unsigned char*> m_pPixelMaps;

};

#endif // ANM_FILE_H

