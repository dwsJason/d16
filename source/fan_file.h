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

typedef struct FAN_Color
{
    unsigned char b;
	unsigned char g;
	unsigned char r;
	unsigned char a;
} FAN_Color;

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

private:

	int m_widthPixels;		// Width of image in pixels
	int m_heightPixels;		// Height of image in pixels
	int m_numColors;		// number of colors in the initial CLUT

	FAN_Palette m_pal;

};


#endif // FAN_FILE_H

