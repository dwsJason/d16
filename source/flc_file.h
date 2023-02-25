//
// C++ Decoder
// For FLC/FLI, Autodesk File Format
// 
//
#ifndef FLC_FILE_H
#define FLC_FILE_H

#include "bctypes.h"
#include <cstdio>
#include <vector>

#include "flic.h"

//------------------------------------------------------------------------------

class FlcFile
{
public:
	// Load an Anm File
	FlcFile(const char *pFilePath);

	~FlcFile();

	// Creation
//	FlcFile(int iWidthPixels, int iHeightPixels, int iFrameSizeBytes);
//	void AddImages( const std::vector<unsigned char*>& pFrameBytes );
	void SaveToFile(const char* pFilenamePath);


	// Retrieval
	void LoadFromFile(const char* pFilePath);
	int GetFrameCount() { return (int)m_pPixelMaps.size(); }
	int GetWidth()  { return m_widthPixels; }
	int GetHeight() { return m_heightPixels; }

//	const ANM_Palette& GetPalette() { return m_pal; }
	const std::vector<unsigned char*> GetPixelMaps() { return m_pPixelMaps; }

private:

	int m_widthPixels;		// Width of image in pixels
	int m_heightPixels;		// Height of image in pixels
	int m_numColors;		// number of colors in the initial CLUT

//	ANM_Palette m_pal;

	std::vector<unsigned char*> m_pPixelMaps;

};

#endif // FLC_FILE_H

