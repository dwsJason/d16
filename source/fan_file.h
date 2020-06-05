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

} FanFile_Header;

// Color LookUp Table, Chunk
typedef struct FanFile_CLUT
{
	char		  c,l,u,t;		// 'C','L','U','T'
	unsigned int  chunk_length; // in bytes, including the 8 bytes header of this chunk

	// BGR triples follow

} FanFile_CLUT;

// INITial Frame Chunk
typedef struct FanFile_INIT
{
	char		  i,n,I,t;		// 'I','N','I','T'
	unsigned int  chunk_length; // in bytes, including the 9 bytes header of this chunk
	unsigned char num_blobs;	// number of blobs to decompress

	// Commands Coded Data Follows

} FanFile_INIT;

// Frames Chunk
typedef struct FanFile_FRAM
{
	char		  f,r,a,m;		// 'F','R','A','M'
	unsigned int  chunk_length; // in bytes, including the 8 bytes header of this chunk

	// Commands Coded Data Follows

} FanFile_FRAM;

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
	~FanFile();

	void SetPalette( const FAN_Palette& palette );
	void AddImages( const std::vector<unsigned char*>& pPixelMaps );

	void SaveToFile(const char* pFilenamePath);

private:

	int EncodeFrame(unsigned char* pCanvas, unsigned char* pFrame, unsigned char* pWorkBuffer, size_t bufferSize );

	int m_widthPixels;		// Width of image in pixels
	int m_heightPixels;		// Height of image in pixels
	int m_numColors;		// number of colors in the initial CLUT

	FAN_Palette m_pal;

	std::vector<unsigned char*> m_pPixelMaps;

};


#endif // FAN_FILE_H

