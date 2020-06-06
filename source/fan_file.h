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
#ifndef FAN_FILE_H
#define FAN_FILE_H

#include <vector>

#pragma pack(push, 1)

typedef struct FAN_Color
{
    unsigned char b;
	unsigned char g;
	unsigned char r;
	unsigned char a;
} FAN_Color;

// Header Chunk (whole file Chunk)
typedef struct FanFile_Header
{
	char 			f,a,n,m;  // 'F','A','N','M'
	unsigned char	version;  // 0x00 or 0x80 for Tiled
	short			width;	  // In pixels
	short			height;	  // In pixels
	unsigned int 	file_length;  // In bytes, including the 16 byte header

	unsigned short	frame_count;	// 3 bytes for the frame count
	unsigned char   frame_count_high;

//------------------------------------------------------------------------------
// If you're doing C, just get rid of these methods
	bool IsValid(unsigned int fileLength)
	{
		if (file_length != fileLength)
			return false;				// size isn't right

		if ((version != 0) && (version != 0x80))
			return false;				// version is not right

		if ((f!='F')||(a!='A')||(n!='N')||(m!='M'))
			return false;				// signature is not right

		if ((0==width)||(0==height))
			return false;				// invalid dimensions

		//unsigned long long pixCount = width * height;

		// Going to put a 4MB limit on the frame size, which is stupid big
		//if (pixCount > (0x400000))
		//	return false;

		return true;
	}

} FanFile_Header;

// Color LookUp Table, Chunk
typedef struct FanFile_CLUT
{
	char		  c,l,u,t;		// 'C','L','U','T'
	unsigned int  chunk_length; // in bytes, including the 8 bytes header of this chunk

	// BGR triples follow

//------------------------------------------------------------------------------
// If you're doing C, just get rid of these methods
	bool IsValid()
	{
		if (chunk_length > (sizeof(FanFile_CLUT)+(256*3)))
			return false;				// size isn't right

		if ((c!='C')||(l!='L')||(u!='U')||(t!='T'))
			return false;				// signature is not right

		return true;
	}


} FanFile_CLUT;

// INITial Frame Chunk
typedef struct FanFile_INIT
{
	char		  i,n,I,t;		// 'I','N','I','T'
	unsigned int  chunk_length; // in bytes, including the 9 bytes header of this chunk
	unsigned char num_blobs;	// number of blobs to decompress

	// Commands Coded Data Follows
//------------------------------------------------------------------------------
// If you're doing C, just get rid of these methods
	bool IsValid()
	{
		if ((i!='I')||(n!='N')||(I!='I')||(t!='T'))
			return false;				// signature is not right

		return true;
	}


} FanFile_INIT;

// Frames Chunk
typedef struct FanFile_FRAM
{
	char		  f,r,a,m;		// 'F','R','A','M'
	unsigned int  chunk_length; // in bytes, including the 8 bytes header of this chunk

	// Commands Coded Data Follows

//------------------------------------------------------------------------------
// If you're doing C, just get rid of these methods
	bool IsValid()
	{
		if ((f!='F')||(r!='R')||(a!='A')||(m!='M'))
			return false;				// signature is not right

		return true;
	}

} FanFile_FRAM;

// Generic Unknown Chunk
typedef struct FanFile_CHUNK
{
	char id0,id1,id2,id3;
	unsigned int chunk_length;

} FanFile_CHUNK;

#pragma pack(pop)


typedef struct FAN_Palette
{
    int iNumColors;
    FAN_Color* pColors;

} FAN_Palette;

class FanFile
{
public:
	// Create a Blank Fan File
	FanFile(int iWidthPixels, int iHeightPixels, int iNumColors);
	// Load in a Fan File
	FanFile(const char *pFilePath);

	~FanFile();

	// Creation
	void SetPalette( const FAN_Palette& palette );
	void AddImages( const std::vector<unsigned char*>& pPixelMaps );
	void SaveToFile(const char* pFilenamePath);

	// Retrieval
	void LoadFromFile(const char* pFilePath);
	int GetFrameCount() { return (int)m_pPixelMaps.size(); }
	int GetWidth()  { return m_widthPixels; }
	int GetHeight() { return m_heightPixels; }

	const FAN_Palette& GetPalette() { return m_pal; }
	const std::vector<unsigned char*> GetPixelMaps() { return m_pPixelMaps; }

private:

	void UnpackClut(FanFile_CLUT* pCLUT);
	void UnpackInitialFrame(FanFile_INIT* pINIT);
	void UnpackFrames(FanFile_FRAM* pFRAM);

	int EncodeFrame(unsigned char* pCanvas, unsigned char* pFrame, unsigned char* pWorkBuffer, size_t bufferSize );

	int m_widthPixels;		// Width of image in pixels
	int m_heightPixels;		// Height of image in pixels
	int m_numColors;		// number of colors in the initial CLUT

	FAN_Palette m_pal;

	std::vector<unsigned char*> m_pPixelMaps;

};


#endif // FAN_FILE_H

