//
// C++ Encoder/Decoder
// For C256 Foenix Picture File Format
//
//  $$TODO, bring down a text file copy of the spec into the project
// https://docs.google.com/document/d/10ovgMClDAJVgbW0sOhUsBkVABKWhOPM5Au7vbHJymoA/edit?usp=sharing
//
#ifndef C256_FILE_H
#define C256_FILE_H

#include <vector>

#pragma pack(push, 1)

typedef struct C256_Color
{
    unsigned char b;
	unsigned char g;
	unsigned char r;
	unsigned char a;
} C256_Color;

// Header Chunk (whole file Chunk)
typedef struct C256File_Header
{
	char 			hi,h2,h5,h6;  // 'I','2','5','6'

	unsigned int 	file_length;  // In bytes, including the 16 byte header

	short 			version;  // 0x0000 for now
	short			width;	  // In pixels
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

		if ((hi!='I')||(h2!='2')||(h5!='5')||(h6!='6'))
			return false;				// signature is not right

		if ((0==width)||(0==height))
			return false;				// invalid dimensions

		if (reserved != 0)				// reserved field is reserved
			return false;

		return true;
	}

} C256File_Header;

// PIXeL chunk
typedef struct C256File_PIXL
{
	char		  p,i,x,l;		// 'P','I','X','L'
	unsigned int  chunk_length; // in bytes, including the 9 bytes header of this chunk
	unsigned short num_blobs;	// number of blobs to decompress

	// Commands Coded Data Follows
//------------------------------------------------------------------------------
// If you're doing C, just get rid of these methods
	bool IsValid()
	{
		if ((p!='P')||(i!='I')||(x!='X')||(l!='L'))
			return false;				// signature is not right

		return true;
	}


} C256File_PIXL;


// Color LookUp Table, Chunk
typedef struct C256File_CLUT
{
	char		  c,l,u,t;		// 'C','L','U','T'
	unsigned int  chunk_length; // in bytes, including the 8 bytes header of this chunk
	unsigned short num_colors;  // number of colors-1, 1-16384 colors

	// BGRA quads follow, either raw or lzsa2 compressed

//------------------------------------------------------------------------------
// If you're doing C, just get rid of these methods
	bool IsValid()
	{
		int numColors = num_colors & 0x3FFF;
		numColors++;

		if (chunk_length > (sizeof(C256File_CLUT)+(numColors*4)))
			return false;				// size isn't right

		if ((c!='C')||(l!='L')||(u!='U')||(t!='T'))
			return false;				// signature is not right

		return true;
	}


} C256File_CLUT;



// Generic Unknown Chunk
typedef struct C256File_CHUNK
{
	char id0,id1,id2,id3;
	unsigned int chunk_length;

} C256File_CHUNK;

#pragma pack(pop)


typedef struct C256_Palette
{
    int iNumColors;
    C256_Color* pColors;

} C256_Palette;

class C256File
{
public:
	// Create a Blank Fan File
	C256File(int iWidthPixels, int iHeightPixels, int iNumColors);
	// Load in a C256 Image File
	C256File(const char *pFilePath);

	~C256File();

	// Creation
	void SetPalette( const C256_Palette& palette );
	void AddImages( const std::vector<unsigned char*>& pPixelMaps );
	void SaveToFile(const char* pFilenamePath);

	// Retrieval
	void LoadFromFile(const char* pFilePath);
	int GetFrameCount() { return (int)m_pPixelMaps.size(); }
	int GetWidth()  { return m_widthPixels; }
	int GetHeight() { return m_heightPixels; }

	const C256_Palette& GetPalette() { return m_pal; }
	const std::vector<unsigned char*> GetPixelMaps() { return m_pPixelMaps; }

private:

	void UnpackClut(C256File_CLUT* pCLUT);
	void UnpackPixel(C256File_PIXL* pPIXL);

	void CombinePixelMaps();


//	int EncodeFrame(unsigned char* pCanvas, unsigned char* pFrame, unsigned char* pWorkBuffer, size_t bufferSize );

	int m_widthPixels;		// Width of image in pixels
	int m_heightPixels;		// Height of image in pixels
	int m_numColors;		// number of colors in the initial CLUT

	C256_Palette m_pal;

	std::vector<unsigned char*> m_pPixelMaps;

};


#endif // C256_FILE_H

