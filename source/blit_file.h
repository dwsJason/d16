//
// C++ Encoder/Decoder
// For BLIT, compiled sprite thing
// 
#ifndef BLIT_FILE_H
#define BLIT_FILE_H

class BLITFile
{
public:
	// Load in a GSLA File
	BLITFile(const char *pFilePath);
	~BLITFile();

	// Creation
	BLITFile(int iWidthPixels, int iHeightPixels, int iFrameSizeBytes);
	void AddImages( const std::vector<unsigned char*>& pFrameBytes );
	void SaveToFile(const char* pFilenamePath);

private:

	int m_frameSize;  	    // frame buffer size in bytes

	int m_widthPixels;		// Width of image in pixels
	int m_heightPixels;		// Height of image in pixels

	std::vector<unsigned char*> m_pC1PixelMaps;

};

#endif // BLIT_FILE_H

