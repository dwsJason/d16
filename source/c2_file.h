//
// C++ Encoder/Decoder
// For C2, Paintworks Animation File Format
//
// File Format summary here from Brutal Deluxe
//
//0000.7fff first pic
//8000.8003 length of frame data - 8008
//8004.8007 timing
//8008.800b length of first frame data
//800c.800d first frame index
//800e.800f first frame data 
//
// Offset 0, Value FFFF indicates the end of a frame
// 	00 00 FF FF 
//
#ifndef C2_FILE_H
#define C2_FILE_H

#include <vector>

#pragma pack(push, 1)

typedef struct C2File_Header
{
	char 			image[0x8000];   // C1 Initial Image

	unsigned int 	file_length;  // length of frame_data (file length - 8008)
	unsigned int 	timing;
	unsigned int	frame_length; // first encoded frame length

//------------------------------------------------------------------------------
// If you're doing C, just get rid of these methods
	bool IsValid(unsigned int fileLength)
	{
		if (file_length != fileLength)
			return false;				// size isn't right

		return true;
	}

} C2File_Header;

#pragma pack(pop)

class C2File
{
public:
	// Create a Blank Fan File
	//C2File();
	// Load in a Fan File
	C2File(const char *pFilePath);

	~C2File();

	// Creation
	//void SetPalette( const FAN_Palette& palette );
	//void AddImages( const std::vector<unsigned char*>& pPixelMaps );
	//void SaveToFile(const char* pFilenamePath);

	// Retrieval
	void LoadFromFile(const char* pFilePath);
	int GetFrameCount() { return (int)m_pPixelMaps.size(); }
	int GetWidth()  { return m_widthPixels; }
	int GetHeight() { return m_heightPixels; }

	//const FAN_Palette& GetPalette() { return m_pal; }
	const std::vector<unsigned char*> GetPixelMaps() { return m_pPixelMaps; }

private:

	//void UnpackClut(FanFile_CLUT* pCLUT);
	//void UnpackInitialFrame(FanFile_INIT* pINIT);
	//void UnpackFrames(FanFile_FRAM* pFRAM);

	//int EncodeFrame(unsigned char* pCanvas, unsigned char* pFrame, unsigned char* pWorkBuffer, size_t bufferSize );

	int m_widthPixels;		// Width of image in pixels
	int m_heightPixels;		// Height of image in pixels

	std::vector<unsigned char*> m_pPixelMaps;

};


#endif // C2_FILE_H

