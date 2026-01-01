//
// https://rosettacode.org/wiki/Xiaolin_Wu%27s_line_algorithm
//

#ifndef XU_LINE_H
#define XU_LINE_H

#include <SDL.h>
#include "bctypes.h"
#include "obj_file.h"

class CRawCanvas
{
public:

	CRawCanvas(COBJFile* pOBJFile);

	CRawCanvas(int width, int height)
		: m_width(width)
		, m_height(height)
	{
		m_color = 0xFFFFFFFF; // rgba
		m_pRawPixels = new u32[ m_width * m_height ];
		m_pOBJFile = nullptr;
	}

	~CRawCanvas()
	{
		if (m_pRawPixels)
		{
			delete m_pRawPixels;
			m_pRawPixels = nullptr;
		}

		if (m_pOBJFile)
		{
			delete m_pOBJFile;
			m_pOBJFile = nullptr;
		}
	}

	// rendering mode, to generate explosion animation
	// for Odds video game
	std::vector<SDL_Surface*> RenderExplosion();

//-----------------------------------------------------------------------------

	std::vector<SDL_Surface*> RenderFrames();

	u32* m_pRawPixels;
	u32  m_color;

	int m_width;
	int m_height;

	void WULine(float x0, float y0, float x1, float y1);
	void BLine(int x0, int y0, int x1, int y1);

private:

	COBJFile* m_pOBJFile;

	void inline plotf(i16 x, i16 y, float alpha);
	void inline setPixel(i16 x, i16 y);

	SDL_Surface* SDL_SurfaceFromRawRGBA(Uint32 *pPixels, int iWidth, int iHeight);

};


#endif // XU_LINE_H

