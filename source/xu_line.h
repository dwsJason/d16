//
// https://rosettacode.org/wiki/Xiaolin_Wu%27s_line_algorithm
//

#ifndef XU_LINE_H
#define XU_LINE_H

#include <SDL.h>
#include "bctypes.h"

class CRawCanvas
{
public:
	u32* m_pSurface;
	u32  m_color;

	int m_width;
	int m_height;

	void WULine(float x0, float y0, float x1, float y1);

private:

	void inline plotf(i16 x, i16 y, float alpha);


};


#endif // XU_LINE_H

