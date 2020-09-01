//
// C++ Encoder
// For GSdx, a tool for pre-compiling scroll code on the IIgs
// 
// File Format summary here from Brutal Deluxe
//
//

#ifndef GSDX_FILE_H
#define GSDX_FILE_H

#include <vector>

// Prototypes
struct SDL_Surface;


class GSDXFile
{
public:
	GSDXFile(SDL_Surface* pImage);

	~GSDXFile();


	void GenerateDeltaY( int deltaY, const char* pFilePath );

private:

	unsigned char* GetC1StylePixels( SDL_Surface* pImage );
	void CompileLine(unsigned char* pDest, unsigned char* pSource,
					 std::vector<unsigned char>& code,
					 std::vector<unsigned char>& table );




	SDL_Surface* m_pBaseImage;

};


#endif // GSDX_FILE_H

