//
// C++ Encoder/Decoder
// For BLIT, compiled sprite thing
// 
#ifndef BLIT_FILE_H
#define BLIT_FILE_H

#include <vector>

#include "imgui.h"
#include "SDL_Surface.h"

class BLITFile
{
public:
	// Creation
	BLITFile(const std::vector<SDL_Surface*>& pSurfaces, const std::vector<ImVec4>& targetColors);
	~BLITFile();

	void AddImages( const std::vector<unsigned char*>& pFrameBytes );
	void SaveToFile(const char* pFilenamePath);

private:

	Uint32 ClosestIndex(Uint32* pClut, Uint32 uColor, Uint32 uNumColors=16);
	unsigned char* CreateC1Data(SDL_Surface* pImage);
	Uint32 SDL_GetPixel(SDL_Surface* pSurface, int x, int y);


	int m_frameSize;  	    // frame buffer size in bytes

	int m_widthPixels;		// Width of image in pixels
	int m_heightPixels;		// Height of image in pixels

	std::vector<ImVec4> m_targetColors;
	std::vector<unsigned char*> m_pC1PixelMaps;

};

#endif // BLIT_FILE_H

