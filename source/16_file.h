//
// C++ Encoder/Decoder
// For a GS 16 color index Picture File Format
//
//  $$TODO, spec file for this 16 color file format
//  $$TODO, bring down a text file copy of the spec into the project
// https://docs.google.com/document/d/10ovgMClDAJVgbW0sOhUsBkVABKWhOPM5Au7vbHJymoA/edit?usp=sharing
//
// On-disk pixel layout (PIXL chunk):
//   Each row of width N pixels packs into ceil(N/2) bytes.
//   For an even pixel column x, the color index is the HIGH nibble of the byte.
//   For an odd  pixel column x, the color index is the LOW  nibble of the byte.
//
#ifndef C16_FILE_H
#define C16_FILE_H

#include <stdint.h>
#include <vector>

#pragma pack(push, 1)

// 4-bits-per-channel packed BGRA color (Apple IIgs / Foenix native palette entry).
// 16 bits total = 2 bytes per color.
typedef struct C16_Color
{
	uint16_t b : 4;
	uint16_t g : 4;
	uint16_t r : 4;
	uint16_t a : 4;
} C16_Color;

// Header Chunk (whole file Chunk)
typedef struct C16File_Header
{
	char 			hi,h2,h5,h6;  // 'I','1','6','I'

	unsigned int 	file_length;  // In bytes, including the 16 byte header

	short 			version;  // 0x0000 for now
	short			width;	  // In bytes (on-disk byte width per scanline)
	short			height;	  // In pixels

	short			reserved; // Reserved for future expansion, set to 0

//------------------------------------------------------------------------------
// If you're doing C, just get rid of these methods
	bool IsValid(unsigned int fileLength)
	{
		if (file_length != fileLength)
			return false;				// size isn't right

		if (version != 0)
			return false;				// version is not right

		if ((hi!='I')||(h2!='1')||(h5!='6')||(h6!='I'))
			return false;				// signature is not right

		if ((0==width)||(0==height))
			return false;				// invalid dimensions

		if (reserved != 0)				// reserved field is reserved
			return false;

		return true;
	}

} C16File_Header;

// PIXeL chunk -- nibble-packed pixel data
typedef struct C16File_PIXL
{
	char		  p,i,x,l;		// 'P','I','X','L'
	unsigned int  chunk_length; // in bytes, including the 10 bytes header of this chunk
	unsigned short num_blobs;	// number of 64KB lzsa2 blobs that follow

	// Compressed blobs of nibble-packed pixel bytes follow
//------------------------------------------------------------------------------
// If you're doing C, just get rid of these methods
	bool IsValid()
	{
		if ((p!='P')||(i!='I')||(x!='X')||(l!='L'))
			return false;				// signature is not right

		return true;
	}


} C16File_PIXL;

// Scanline Control Byte Chunk
typedef struct C16File_SCBs
{
	char		S,c,b,s;		// 'S','C','B','s'
	unsigned int	chunk_length;  // in bytes, including the 10 bytes header of this chunk
	unsigned short  num_scbs;

//------------------------------------------------------------------------------
// If you're doing C, just get rid of these methods
	bool IsValid()
	{
		if ((S!='S')||(c!='C')||(b!='B')||(s!='s'))
			return false;				// signature is not right

		return true;
	}

} C16File_SCBs;


// Color LookUp Table, Chunk
typedef struct C16File_CLUT
{
	char		  c,l,u,t;		// 'C','L','U','T'
	unsigned int  chunk_length; // in bytes, including the 10 bytes header of this chunk
	unsigned short num_colors;  // low 15 bits = number of colors (1-16 for I16);
								// high bit (0x8000) = compressed flag

	// Packed 16-bit BGRA colors follow (2 bytes each), either raw or lzsa2 compressed

//------------------------------------------------------------------------------
// If you're doing C, just get rid of these methods
	bool IsValid()
	{
		int numColors = num_colors & 0x7FFF;

		if (chunk_length > (sizeof(C16File_CLUT)+(numColors*2)))
			return false;				// size isn't right

		if ((c!='C')||(l!='L')||(u!='U')||(t!='T'))
			return false;				// signature is not right

		return true;
	}


} C16File_CLUT;



// Generic Unknown Chunk
typedef struct C16File_CHUNK
{
	char id0,id1,id2,id3;
	unsigned int chunk_length;

} C16File_CHUNK;

#pragma pack(pop)


typedef struct C16_Palette
{
	int iNumColors;
	C16_Color* pColors;

} C16_Palette;

typedef struct C16_SCB
{
	int iNumScanLines;
	uint8_t* pSCB;
} C16_SCB;

class C16File
{
public:
	// Create a Blank 16 File
	C16File(int iWidthBytes, int iHeightPixels, int iNumColors);
	// Load in a C16 Image File
	C16File(const char *pFilePath);

	~C16File();

	// Creation
	void SetPalette( const C16_Palette& palette );
	// Caller-trusted: iNumScanLines isn't validated against m_heightPixels.
	// IIgs convention is one SCB per scanline (bit 7 = 320/640 mode,
	// bits 3-0 = palette index 0-15).
	void SetSCBs( const C16_SCB& scbs );
	void AddImages( const std::vector<unsigned char*>& pPixelMaps );
	void SaveToFile(const char* pFilenamePath);

	// Retrieval
	void LoadFromFile(const char* pFilePath);
	int GetFrameCount() { return (int)m_pPixelMaps.size(); }
	int GetWidthBytes()  { return m_widthBytes; }
	// Default 16-color/320-mode assumption: 2 pixels per byte.
	int GetWidthPixels() { return m_widthBytes * 2; }
	int GetHeight() { return m_heightPixels; }

	const C16_Palette& GetPalette() { return m_pal; }
	// iNumScanLines == 0 when the file has no SCBs chunk.
	const C16_SCB& GetSCBs() { return m_scb; }
	const std::vector<unsigned char*>& GetPixelMaps() { return m_pPixelMaps; }

private:

	void UnpackClut(C16File_CLUT* pCLUT);
	void UnpackPixel(C16File_PIXL* pPIXL);
	void UnpackSCBs(C16File_SCBs* pSCBs);

	void CombinePixelMaps();

	// Nibble pack/unpack helpers (high nibble = even x, low nibble = odd x)
	static void NibblePack(const unsigned char* pSrc, unsigned char* pDst,
						   int widthPixels, int heightPixels);
	static void NibbleUnpack(const unsigned char* pSrc, unsigned char* pDst,
							 int widthPixels, int heightPixels);

	int m_widthBytes;		// Width of image in bytes (on-disk scanline width)
	int m_heightPixels;		// Height of image in pixels
	int m_numColors;		// number of colors in the initial CLUT

	C16_Palette m_pal;
	C16_SCB     m_scb;   // iNumScanLines == 0 when no SCBs chunk is present

	std::vector<unsigned char*> m_pPixelMaps;
};


#endif // C16_FILE_H

