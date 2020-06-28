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

	int m_widthPixels;		// Width of image in pixels
	int m_heightPixels;		// Height of image in pixels
	int m_numColors;		// number of colors in the initial CLUT

	ANM_Palette m_pal;

	std::vector<unsigned char*> m_pPixelMaps;

};

#endif // ANM_FILE_H

